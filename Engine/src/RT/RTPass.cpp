#include "RT/RTPass.h"
#include "Vulkan/ShaderLoader.h"
#include "Core/Log.h"
#include <algorithm>
#include <cstring>
#include <exception>

namespace RT
{

namespace
{

constexpr VkShaderStageFlags kRgen = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
constexpr VkShaderStageFlags kHits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

VkShaderModule makeModule(VkDevice dev, const char* name)
{
    std::vector<uint32_t> code;
    try { code = loadSpirvFile(name); }
    catch (const std::exception& e) { LOG("RT: shader missing: " << name << " (" << e.what() << ")\n"); }
    if (code.empty()) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode = code.data();
    VkShaderModule m = VK_NULL_HANDLE;
    vkCreateShaderModule(dev, &ci, nullptr, &m);
    return m;
}

uint32_t alignUp(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

void fullBarrier(VkCommandBuffer vk)
{
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &mb, 0, nullptr, 0, nullptr);
}

void transition(ASH::CommandBuffer* cmd, ASH::Texture* t, ASH::ResourceState from, ASH::ResourceState to)
{
    if (!t) return;
    ASH::TextureBarrier tb{};
    tb.texture = t;
    tb.oldState = from;
    tb.newState = to;
    cmd->barrier(&tb, 1, nullptr, 0);
}

}

bool Pass::create(Context& ctx, uint32_t frames, uint32_t w, uint32_t h)
{
    m_frames = frames;
    m_width = w;
    m_height = h;
    m_outDesc = outputs();

    if (!createLayouts(ctx) || !createPipelineAndSbt(ctx) || !createSets(ctx))
    {
        LOG("RT: pass " << raygenName() << " create FAILED\n");
        destroy(ctx);
        return false;
    }

    m_cam.assign(frames, nullptr);
    m_light.assign(frames, nullptr);
    createImages(ctx);
    updateAllSets(ctx);
    return true;
}

bool Pass::createLayouts(Context& ctx)
{
    VkDevice dev = ctx.device();

    std::vector<VkDescriptorSetLayoutBinding> b = {
        { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, kRgen, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kRgen, nullptr },
        { 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kRgen, nullptr },
        { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kHits, nullptr },
        { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxTextures, kHits, nullptr },
        { 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxTextures, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    };
    std::vector<VkDescriptorBindingFlags> f(b.size(), 0);
    f[4] = f[5] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    for (const auto& o : m_outDesc)
    {
        b.push_back({ o.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, kRgen, nullptr });
        f.push_back(0);
    }
    if (m_gbuf)
    {
        b.push_back({ kGBufBinding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, kRgen, nullptr });
        f.push_back(0);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bf{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bf.bindingCount = uint32_t(f.size());
    bf.pBindingFlags = f.data();

    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.pNext = &bf;
    lci.bindingCount = uint32_t(b.size());
    lci.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &m_setLayout) != VK_SUCCESS) return false;

    VkPushConstantRange pcr{kRgen, 0, sizeof(uint32_t) * 2};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    return vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipeLayout) == VK_SUCCESS;
}

bool Pass::createPipelineAndSbt(Context& ctx)
{
    VkDevice dev = ctx.device();

    const char* names[4] = { raygenName(), "rt_hit.rmiss", "rt_hit.rchit", "rt_hit.rahit" };
    const VkShaderStageFlagBits bits[4] = {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_SHADER_STAGE_ANY_HIT_BIT_KHR };

    VkShaderModule mods[4]{};
    VkPipelineShaderStageCreateInfo stages[4]{};
    bool ok = true;
    for (int i = 0; i < 4; ++i)
    {
        mods[i] = makeModule(dev, names[i]);
        if (!mods[i]) ok = false;
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = bits[i];
        stages[i].module = mods[i];
        stages[i].pName = "main";
    }

    VkRayTracingShaderGroupCreateInfoKHR g[3]{};
    for (auto& x : g)
    {
        x.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        x.generalShader = x.closestHitShader = x.anyHitShader = x.intersectionShader = VK_SHADER_UNUSED_KHR;
    }
    g[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; g[0].generalShader = 0;
    g[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; g[1].generalShader = 1;
    g[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    g[2].closestHitShader = 2;
    g[2].anyHitShader = 3;

    if (ok)
    {
        VkRayTracingPipelineCreateInfoKHR ci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
        ci.stageCount = 4;
        ci.pStages = stages;
        ci.groupCount = 3;
        ci.pGroups = g;
        ci.maxPipelineRayRecursionDepth = 1;
        ci.layout = m_pipeLayout;
        ok = ctx.vkCreateRTPipelines(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipeline) == VK_SUCCESS;
    }
    for (auto m : mods) if (m) vkDestroyShaderModule(dev, m, nullptr);
    if (!ok) return false;

    const auto& p = ctx.rtProps();
    const uint32_t hs = p.shaderGroupHandleSize;
    const uint32_t baseAlign = p.shaderGroupBaseAlignment;
    const uint32_t stride = alignUp(hs, p.shaderGroupHandleAlignment);
    const uint32_t groups = 3;

    std::vector<uint8_t> handles(size_t(groups) * hs);
    if (ctx.vkGetGroupHandles(dev, m_pipeline, 0, groups, handles.size(), handles.data()) != VK_SUCCESS)
        return false;

    const uint32_t rgenSize = alignUp(stride, baseAlign);
    const uint32_t missOff = rgenSize;
    const uint32_t hitOff = missOff + alignUp(stride, baseAlign);
    const size_t total = size_t(hitOff) + alignUp(stride, baseAlign);

    m_sbt = ctx.createBuffer(total + baseAlign, ASH::BufferUsage::ShaderBindingTable,
        ASH::MemoryUsage::CpuToGpu, raygenName());
    const uint64_t base = m_sbt->getDeviceAddress();
    const uint64_t aligned = (base + baseAlign - 1) & ~uint64_t(baseAlign - 1);
    uint8_t* dst = static_cast<uint8_t*>(m_sbt->map()) + (aligned - base);
    std::memset(dst, 0, total);
    std::memcpy(dst,          handles.data() + 0 * hs, hs);
    std::memcpy(dst + missOff, handles.data() + 1 * hs, hs);
    std::memcpy(dst + hitOff,  handles.data() + 2 * hs, hs);

    m_rgen = { aligned, rgenSize, rgenSize };
    m_miss = { aligned + missOff, stride, stride };
    m_hit  = { aligned + hitOff, stride, stride };
    return true;
}

bool Pass::createSets(Context& ctx)
{
    VkDevice dev = ctx.device();
    const uint32_t n = m_frames;
    const uint32_t outCount = std::max<uint32_t>(uint32_t(m_outDesc.size()) + (m_gbuf ? 1u : 0u), 1u);

    const VkDescriptorPoolSize sizes[5] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, n },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, n * 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, n },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n * kMaxTextures * 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, n * outCount },
    };
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = n;
    pci.poolSizeCount = 5;
    pci.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(dev, &pci, nullptr, &m_pool) != VK_SUCCESS) return false;

    std::vector<VkDescriptorSetLayout> layouts(n, m_setLayout);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = n;
    ai.pSetLayouts = layouts.data();
    m_sets.assign(n, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(dev, &ai, m_sets.data()) != VK_SUCCESS) return false;

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxLod = 0.0f;
    return vkCreateSampler(dev, &si, nullptr, &m_sampler) == VK_SUCCESS;
}

void Pass::createImages(Context& ctx)
{
    m_out.clear();
    m_out.resize(m_outDesc.size());
    for (size_t i = 0; i < m_outDesc.size(); ++i)
    {
        for (uint32_t f = 0; f < m_frames; ++f)
        {
            ASH::TextureDesc d{};
            d.type = ASH::TextureType::Texture2D;
            d.format = m_outDesc[i].format;
            d.extent = { m_width, m_height, 1 };
            d.mipLevels = 1; d.arrayLayers = 1; d.sampleCount = 1;
            d.usage = ASH::TextureUsage::Storage | ASH::TextureUsage::Sampled;
            d.debugName = m_outDesc[i].name;
            m_out[i].push_back(ctx.ash()->createTexture(d));
        }
    }
    onImagesCreated(ctx);
}

void Pass::updateSet(Context& ctx, uint32_t frame)
{
    if (frame >= m_sets.size() || !m_sets[frame]) return;

    DescriptorWriter dw(m_sets[frame]);
    dw.accelerationStructure(0, m_tlas)
      .buffer(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame < m_cam.size() ? m_cam[frame] : nullptr)
      .buffer(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame < m_light.size() ? m_light[frame] : nullptr)
      .buffer(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_geoBuf);
    for (size_t i = 0; i < m_outDesc.size(); ++i)
        dw.image(m_outDesc[i].binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, out(i, frame), VK_IMAGE_LAYOUT_GENERAL);
    if (m_gbuf)
        dw.image(kGBufBinding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_gbuf->image(frame), VK_IMAGE_LAYOUT_GENERAL);
    dw.commit(ctx.device());

    if (m_albedo.empty() || m_albedo.size() != m_normal.size() || !m_sampler) return;

    const uint32_t count = std::min<uint32_t>(uint32_t(m_albedo.size()), kMaxTextures);
    std::vector<VkDescriptorImageInfo> a(count), nrm(count);
    for (uint32_t k = 0; k < count; ++k)
    {
        a[k] = { m_sampler, m_albedo[k], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        nrm[k] = { m_sampler, m_normal[k], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }
    VkWriteDescriptorSet w[2]{};
    for (int j = 0; j < 2; ++j)
    {
        w[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[j].dstSet = m_sets[frame];
        w[j].dstBinding = 5 + j;
        w[j].descriptorCount = count;
        w[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[j].pImageInfo = j == 0 ? a.data() : nrm.data();
    }
    vkUpdateDescriptorSets(ctx.device(), 2, w, 0, nullptr);
}

void Pass::updateAllSets(Context& ctx)
{
    for (uint32_t f = 0; f < m_frames; ++f) updateSet(ctx, f);
}

bool Pass::canTrace(uint32_t frame) const
{
    return ready() && frame < m_frames && frame < m_sets.size()
        && m_tlas && m_geoBuf
        && m_cam[frame] && m_light[frame]
        && !m_albedo.empty()
        && out(0, frame)
        && (!m_gbuf || m_gbuf->image(frame));
}

void Pass::bindFrameBuffers(Context& ctx, uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights)
{
    if (frame >= m_frames) return;
    m_cam[frame] = camera;
    m_light[frame] = lights;
    updateSet(ctx, frame);
}

void Pass::setTlas(Context& ctx, VkAccelerationStructureKHR tlas)
{
    m_tlas = tlas;
    updateAllSets(ctx);
}

void Pass::setGeometry(Context& ctx, ASH::Buffer* geoBuf)
{
    m_geoBuf = geoBuf ? reinterpret_cast<VkBuffer>(geoBuf->getNativeHandle()) : VK_NULL_HANDLE;
    updateAllSets(ctx);
}

void Pass::setTextures(Context& ctx, const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal)
{
    m_albedo = albedo;
    m_normal = normal;
    updateAllSets(ctx);
}

void Pass::resize(Context& ctx, uint32_t w, uint32_t h)
{
    if (!ready() || w == 0 || h == 0 || (w == m_width && h == m_height)) return;
    ctx.ash()->waitIdle();
    m_width = w;
    m_height = h;
    createImages(ctx);
    updateAllSets(ctx);
}

void Pass::record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)
{
    if (!canTrace(frame)) return;
    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());
    using S = ASH::ResourceState;

    fullBarrier(vk);
    for (size_t i = 0; i < m_outDesc.size(); ++i)
        transition(cmd, out(i, frame), S::Undefined, S::General);
    fullBarrier(vk);

    const uint32_t push[2] = { m_frameCounter++, 0u };
    vkCmdPushConstants(vk, m_pipeLayout, kRgen, 0, sizeof(push), push);
    vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    vkCmdBindDescriptorSets(vk, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeLayout,
        0, 1, &m_sets[frame], 0, nullptr);
    ctx.vkCmdTraceRays(vk, &m_rgen, &m_miss, &m_hit, &m_callable, m_width, m_height, 1);

    fullBarrier(vk);
    for (size_t i = 0; i < m_outDesc.size(); ++i)
        if (m_outDesc[i].sampledAfter)
            transition(cmd, out(i, frame), S::General, S::ShaderReadOnly);
}

void Pass::destroy(Context& ctx)
{
    VkDevice dev = ctx.device();
    if (m_pipeline) vkDestroyPipeline(dev, m_pipeline, nullptr);
    if (m_pipeLayout) vkDestroyPipelineLayout(dev, m_pipeLayout, nullptr);
    if (m_setLayout) vkDestroyDescriptorSetLayout(dev, m_setLayout, nullptr);
    if (m_pool) vkDestroyDescriptorPool(dev, m_pool, nullptr);
    if (m_sampler) vkDestroySampler(dev, m_sampler, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    m_pipeLayout = VK_NULL_HANDLE;
    m_setLayout = VK_NULL_HANDLE;
    m_pool = VK_NULL_HANDLE;
    m_sampler = VK_NULL_HANDLE;

    m_sbt.reset();
    m_rgen = m_miss = m_hit = m_callable = {};
    m_sets.clear();
    m_out.clear();
    m_outDesc.clear();
    m_cam.clear();
    m_light.clear();
    m_albedo.clear();
    m_normal.clear();
    m_tlas = VK_NULL_HANDLE;
    m_geoBuf = VK_NULL_HANDLE;
    m_frames = m_width = m_height = 0;
    m_frameCounter = 0;
}

}

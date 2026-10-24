#include "RT/RTReflectionPass.h"
#include "Vulkan/ShaderLoader.h"
#include <cstring>
#include <vector>

namespace RT
{

static VkShaderModule makeModule(VkDevice dev, const char* name)
{
    std::vector<uint32_t> code = loadSpirvFile(name);
    if (code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode = code.data();
    VkShaderModule m = VK_NULL_HANDLE;
    vkCreateShaderModule(dev, &ci, nullptr, &m);
    return m;
}

static uint32_t alignUp(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

bool ReflectionPass::createPipeline(Context& ctx)
{
    VkDevice dev = ctx.device();

    const VkShaderStageFlags hitStages =
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding b[7]{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[2] = {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[3] = {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages, nullptr};
    b[5] = {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxTextures, hitStages, nullptr};
    b[6] = {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxTextures, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr};

    VkDescriptorBindingFlags bflags[7] = { 0, 0, 0, 0, 0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bf{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bf.bindingCount = 7;
    bf.pBindingFlags = bflags;

    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.pNext = &bf;
    lci.bindingCount = 7;
    lci.pBindings = b;
    if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &m_setLayout) != VK_SUCCESS) return false;

    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_setLayout;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipeLayout) != VK_SUCCESS) return false;

    const char* names[4] = { "reflection.rgen", "reflection.rmiss", "reflection.rchit", "reflection.rahit" };
    const VkShaderStageFlagBits bits[4] = {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_SHADER_STAGE_ANY_HIT_BIT_KHR };

    VkShaderModule mods[4]{};
    VkPipelineShaderStageCreateInfo stages[4]{};
    bool ok = true;
    for (int i = 0; i < 4; ++i) {
        mods[i] = makeModule(dev, names[i]);
        if (!mods[i]) ok = false;
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = bits[i];
        stages[i].module = mods[i];
        stages[i].pName = "main";
    }

    VkRayTracingShaderGroupCreateInfoKHR g[3]{};
    for (auto& x : g) {
        x.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        x.generalShader = x.closestHitShader = x.anyHitShader = x.intersectionShader = VK_SHADER_UNUSED_KHR;
    }
    g[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; g[0].generalShader = 0;
    g[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; g[1].generalShader = 1;
    g[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    g[2].closestHitShader = 2;
    g[2].anyHitShader     = 3;

    if (ok) {
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

    return ok && createSbt(ctx);
}

bool ReflectionPass::createSbt(Context& ctx)
{
    const auto& p = ctx.rtProps();
    const uint32_t hs = p.shaderGroupHandleSize;
    const uint32_t baseAlign = p.shaderGroupBaseAlignment;
    const uint32_t stride = alignUp(hs, p.shaderGroupHandleAlignment);
    const uint32_t groups = 3;

    std::vector<uint8_t> handles(size_t(groups) * hs);
    if (ctx.vkGetGroupHandles(ctx.device(), m_pipeline, 0, groups, handles.size(), handles.data()) != VK_SUCCESS)
        return false;

    const uint32_t rgenSize = alignUp(stride, baseAlign);
    const uint32_t missOff = rgenSize;
    const uint32_t missSize = alignUp(stride, baseAlign);
    const uint32_t hitOff = missOff + missSize;
    const size_t total = size_t(hitOff) + alignUp(stride, baseAlign);

    m_sbt = ctx.createBuffer(total + baseAlign, ASH::BufferUsage::ShaderBindingTable,
        ASH::MemoryUsage::CpuToGpu, "RT refl sbt");
    const uint64_t base = m_sbt->getDeviceAddress();
    const uint64_t aligned = (base + baseAlign - 1) & ~uint64_t(baseAlign - 1);
    uint8_t* dst = static_cast<uint8_t*>(m_sbt->map()) + (aligned - base);
    std::memset(dst, 0, total);

    std::memcpy(dst, handles.data() + 0 * hs, hs);
    std::memcpy(dst + missOff, handles.data() + 1 * hs, hs);
    std::memcpy(dst + hitOff, handles.data() + 2 * hs, hs);

    m_rgen = { aligned, rgenSize, rgenSize };
    m_miss = { aligned + missOff, stride, stride };
    m_hit  = { aligned + hitOff, stride, stride };
    return true;
}

void ReflectionPass::createImages(Context& ctx)
{
    m_img.clear();
    for (uint32_t i = 0; i < m_frames; ++i) {
        ASH::TextureDesc d{};
        d.type = ASH::TextureType::Texture2D;
        d.format = ASH::Format::R16G16B16A16_Float;
        d.extent = { m_w, m_h, 1 };
        d.mipLevels = 1; d.arrayLayers = 1; d.sampleCount = 1;
        d.usage = ASH::TextureUsage::Storage | ASH::TextureUsage::Sampled;
        d.debugName = "RT reflection";
        m_img.push_back(ctx.ash()->createTexture(d));
    }
}

bool ReflectionPass::createResources(Context& ctx, uint32_t frames, uint32_t w, uint32_t h)
{
    if (!m_pipeline) return false;

    VkDevice dev = ctx.device();
    m_frames = frames; m_w = w; m_h = h;
    createImages(ctx);

    const VkDescriptorPoolSize sizes[5] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frames },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frames },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames * 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frames },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frames * kMaxTextures * 2 },
    };
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = frames;
    pci.poolSizeCount = 5;
    pci.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(dev, &pci, nullptr, &m_pool) != VK_SUCCESS) return false;

    std::vector<VkDescriptorSetLayout> layouts(frames, m_setLayout);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = frames;
    ai.pSetLayouts = layouts.data();
    std::vector<uint32_t> counts(frames, kMaxTextures);
    
    VkDescriptorSetVariableDescriptorCountAllocateInfo vc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO};
    vc.descriptorSetCount = frames;
    vc.pDescriptorCounts = counts.data();
    ai.pNext = &vc;
    
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxLod = 0.0f;
    if (vkCreateSampler(dev, &si, nullptr, &m_sampler) != VK_SUCCESS) return false;

    m_sets.assign(frames, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(dev, &ai, m_sets.data()) != VK_SUCCESS) return false;

    m_cam.assign(frames, nullptr);
    m_light.assign(frames, nullptr);
    for (uint32_t i = 0; i < frames; ++i) updateSet(ctx, i);
    return true;
}

void ReflectionPass::resize(Context& ctx, uint32_t w, uint32_t h)
{
    if (!m_pipeline || w == 0 || h == 0 || (w == m_w && h == m_h)) return;
    ctx.ash()->waitIdle();
    m_w = w; m_h = h;
    createImages(ctx);
    for (uint32_t i = 0; i < m_frames; ++i) updateSet(ctx, i);
}

void ReflectionPass::bindFrameBuffers(Context& ctx, uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights)
{
    if (frame >= m_frames) return;
    m_cam[frame] = camera;
    m_light[frame] = lights;
    updateSet(ctx, frame);
}

void ReflectionPass::setTlas(Context& ctx, VkAccelerationStructureKHR tlas)
{
    m_tlas = tlas;
    for (uint32_t i = 0; i < m_frames; ++i) updateSet(ctx, i);
}

void ReflectionPass::setGeometry(Context& ctx, ASH::Buffer* geoTable)
{
    m_geoBuf = geoTable ? reinterpret_cast<VkBuffer>(geoTable->getNativeHandle()) : VK_NULL_HANDLE;
    for (uint32_t i = 0; i < m_frames; ++i) updateSet(ctx, i);
}

void ReflectionPass::updateSet(Context& ctx, uint32_t frame)
{
    if (frame >= m_sets.size() || !m_sets[frame]) return;

    std::vector<VkWriteDescriptorSet> w;
    VkWriteDescriptorSetAccelerationStructureKHR asw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    VkDescriptorImageInfo img{};
    VkDescriptorBufferInfo camInfo{}, lightInfo{}, geoInfo{};

    if (m_tlas) {
        asw.accelerationStructureCount = 1;
        asw.pAccelerationStructures = &m_tlas;
        VkWriteDescriptorSet x{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        x.pNext = &asw; x.dstSet = m_sets[frame]; x.dstBinding = 0;
        x.descriptorCount = 1; x.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        w.push_back(x);
    }
    if (frame < m_img.size() && m_img[frame]) {
        img.imageView = reinterpret_cast<VkImageView>(m_img[frame]->getFaceView(0, 0));
        img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet x{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        x.dstSet = m_sets[frame]; x.dstBinding = 1;
        x.descriptorCount = 1; x.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        x.pImageInfo = &img;
        w.push_back(x);
    }
    if (m_cam[frame]) {
        camInfo.buffer = reinterpret_cast<VkBuffer>(m_cam[frame]->getNativeHandle());
        camInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet x{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        x.dstSet = m_sets[frame]; x.dstBinding = 2;
        x.descriptorCount = 1; x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        x.pBufferInfo = &camInfo;
        w.push_back(x);
    }
    if (m_light[frame]) {
        lightInfo.buffer = reinterpret_cast<VkBuffer>(m_light[frame]->getNativeHandle());
        lightInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet x{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        x.dstSet = m_sets[frame]; x.dstBinding = 3;
        x.descriptorCount = 1; x.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        x.pBufferInfo = &lightInfo;
        w.push_back(x);
    }
    if (m_geoBuf) {
        geoInfo.buffer = m_geoBuf;
        geoInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet x{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        x.dstSet = m_sets[frame]; x.dstBinding = 4;
        x.descriptorCount = 1; x.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        x.pBufferInfo = &geoInfo;
        w.push_back(x);
    }
    if (!w.empty()) vkUpdateDescriptorSets(ctx.device(), (uint32_t)w.size(), w.data(), 0, nullptr);
    updateTextures(ctx, frame);
}

static void fullBarrier(VkCommandBuffer vk)
{
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &mb, 0, nullptr, 0, nullptr);
}

void ReflectionPass::record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)
{
        if (!m_pipeline || !m_tlas || !m_geoBuf || frame >= m_frames || !m_cam[frame]) return;

    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());
    ASH::TextureBarrier tb{};
    tb.texture = m_img[frame].get();

    fullBarrier(vk);
    tb.oldState = ASH::ResourceState::Undefined;
    tb.newState = ASH::ResourceState::General;
    cmd->barrier(&tb, 1, nullptr, 0);
    fullBarrier(vk);

    vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    vkCmdBindDescriptorSets(vk, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeLayout,
        0, 1, &m_sets[frame], 0, nullptr);
    ctx.vkCmdTraceRays(vk, &m_rgen, &m_miss, &m_hit, &m_callable, m_w, m_h, 1);

    fullBarrier(vk);
    tb.oldState = ASH::ResourceState::General;
    tb.newState = ASH::ResourceState::ShaderReadOnly;
    cmd->barrier(&tb, 1, nullptr, 0);
}

void ReflectionPass::destroy(Context& ctx)
{
    VkDevice dev = ctx.device();
    m_sbt.reset();
    m_img.clear();
    m_sets.clear();
    if (m_pool)       vkDestroyDescriptorPool(dev, m_pool, nullptr);
    if (m_pipeline)   vkDestroyPipeline(dev, m_pipeline, nullptr);
    if (m_pipeLayout) vkDestroyPipelineLayout(dev, m_pipeLayout, nullptr);
    if (m_setLayout)  vkDestroyDescriptorSetLayout(dev, m_setLayout, nullptr);
    if (m_sampler) vkDestroySampler(dev, m_sampler, nullptr);
    m_sampler = VK_NULL_HANDLE;
    m_views.clear();
    m_pool = VK_NULL_HANDLE; m_pipeline = VK_NULL_HANDLE;
    m_pipeLayout = VK_NULL_HANDLE; m_setLayout = VK_NULL_HANDLE;
    m_rgen = m_miss = m_hit = m_callable = {};
    m_tlas = VK_NULL_HANDLE; m_geoBuf = VK_NULL_HANDLE; m_frames = 0;
}

void ReflectionPass::setTextures(Context& ctx, const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal)
{
    m_views = albedo;
    m_nviews = normal;
    for (uint32_t i = 0; i < m_frames; ++i) updateTextures(ctx, i);
}

void ReflectionPass::updateTextures(Context& ctx, uint32_t frame)
{
    if (frame >= m_sets.size() || !m_sets[frame] || m_views.empty() || !m_sampler) return;
    if (m_nviews.size() != m_views.size()) return;

    std::vector<VkDescriptorImageInfo> a(m_views.size()), n(m_nviews.size());
    for (size_t k = 0; k < m_views.size(); ++k)
    {
        a[k] = { m_sampler, m_views[k], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        n[k] = { m_sampler, m_nviews[k], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }
    VkWriteDescriptorSet w[2]{};
    for (int j = 0; j < 2; ++j)
    {
        w[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[j].dstSet = m_sets[frame];
        w[j].dstBinding = 5 + j;
        w[j].descriptorCount = (uint32_t)a.size();
        w[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[j].pImageInfo = j == 0 ? a.data() : n.data();
    }
    vkUpdateDescriptorSets(ctx.device(), 2, w, 0, nullptr);
}

}
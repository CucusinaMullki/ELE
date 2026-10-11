#include "RT/RTShadowPass.h"
#include "Vulkan/ShaderLoader.h"
#include <cstring>
#include <vector>

namespace RT {

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

bool ShadowPass::createPipeline(Context& ctx)
{
    VkDevice dev = ctx.device();

    VkDescriptorSetLayoutBinding b[4]{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[2] = {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
    b[3] = {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};

    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 4;
    lci.pBindings = b;
    if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &m_setLayout) != VK_SUCCESS) return false;

    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_setLayout;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipeLayout) != VK_SUCCESS) return false;

    const char* names[4] = { "shadow_mask.rgen", "primary.rmiss", "shadow.rmiss", "primary.rchit" };
    const VkShaderStageFlagBits bits[4] = {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
        VK_SHADER_STAGE_MISS_BIT_KHR, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

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

    VkRayTracingShaderGroupCreateInfoKHR g[4]{};
    for (auto& x : g) {
        x.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        x.generalShader = x.closestHitShader = x.anyHitShader = x.intersectionShader = VK_SHADER_UNUSED_KHR;
    }
    for (int i = 0; i < 3; ++i) {
        g[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        g[i].generalShader = i;
    }
    g[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    g[3].closestHitShader = 3;

    if (ok) {
        VkRayTracingPipelineCreateInfoKHR ci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
        ci.stageCount = 4;
        ci.pStages = stages;
        ci.groupCount = 4;
        ci.pGroups = g;
        ci.maxPipelineRayRecursionDepth = 1;
        ci.layout = m_pipeLayout;
        ok = ctx.vkCreateRTPipelines(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipeline) == VK_SUCCESS;
    }
    for (auto m : mods) if (m) vkDestroyShaderModule(dev, m, nullptr);

    return ok && createSbt(ctx);
}

void ShadowPass::createMasks(Context& ctx)
{
    m_mask.clear();
    for (uint32_t i = 0; i < m_frames; ++i) {
        ASH::TextureDesc d{};
        d.type = ASH::TextureType::Texture2D;
        d.format = ASH::Format::R8G8B8A8_UNorm;
        d.extent = { m_w, m_h, 1 };
        d.mipLevels = 1; d.arrayLayers = 1; d.sampleCount = 1;
        d.usage = ASH::TextureUsage::Storage | ASH::TextureUsage::Sampled;
        d.debugName = "RT shadow mask";
        m_mask.push_back(ctx.ash()->createTexture(d));
    }
}

void ShadowPass::resize(Context& ctx, uint32_t w, uint32_t h)
{
    if (!m_pipeline || w == 0 || h == 0 || (w == m_w && h == m_h)) return;
    ctx.ash()->waitIdle();
    m_w = w; m_h = h;
    createMasks(ctx);
    for (uint32_t i = 0; i < m_frames; ++i) updateSet(ctx, i);
}

bool ShadowPass::createResources(Context& ctx, uint32_t frames, uint32_t w, uint32_t h)
{
    VkDevice dev = ctx.device();
    m_frames = frames; m_w = w; m_h = h;

    createMasks(ctx);

    const VkDescriptorPoolSize sizes[3] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frames },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frames },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames * 2 },
    };
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = frames;
    pci.poolSizeCount = 3;
    pci.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(dev, &pci, nullptr, &m_pool) != VK_SUCCESS) return false;

    std::vector<VkDescriptorSetLayout> layouts(frames, m_setLayout);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = frames;
    ai.pSetLayouts = layouts.data();
    m_sets.assign(frames, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(dev, &ai, m_sets.data()) != VK_SUCCESS) return false;

    m_cam.assign(frames, nullptr);
    m_light.assign(frames, nullptr);
    for (uint32_t i = 0; i < frames; ++i) updateSet(ctx, i);
    return true;
}

void ShadowPass::bindFrameBuffers(Context& ctx, uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights)
{
    if (frame >= m_frames) return;
    m_cam[frame] = camera;
    m_light[frame] = lights;
    updateSet(ctx, frame);
}

void ShadowPass::setTlas(Context& ctx, VkAccelerationStructureKHR tlas)
{
    m_tlas = tlas;
    for (uint32_t i = 0; i < m_frames; ++i) updateSet(ctx, i);
}

void ShadowPass::updateSet(Context& ctx, uint32_t frame)
{
    if (frame >= m_sets.size() || !m_sets[frame]) return;

    std::vector<VkWriteDescriptorSet> w;
    VkWriteDescriptorSetAccelerationStructureKHR asw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    VkDescriptorImageInfo img{};
    VkDescriptorBufferInfo camInfo{}, lightInfo{};

    if (m_tlas) {
        asw.accelerationStructureCount = 1;
        asw.pAccelerationStructures = &m_tlas;
        VkWriteDescriptorSet x{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        x.pNext = &asw; x.dstSet = m_sets[frame]; x.dstBinding = 0;
        x.descriptorCount = 1; x.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        w.push_back(x);
    }
    if (frame < m_mask.size() && m_mask[frame]) {
        img.imageView = reinterpret_cast<VkImageView>(m_mask[frame]->getFaceView(0, 0));
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
    if (!w.empty()) vkUpdateDescriptorSets(ctx.device(), (uint32_t)w.size(), w.data(), 0, nullptr);
}

static void fullBarrier(VkCommandBuffer vk)
{
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &mb, 0, nullptr, 0, nullptr);
}

void ShadowPass::record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)
{
    if (!m_pipeline || !m_tlas || frame >= m_frames || !m_cam[frame] || !m_light[frame]) return;

    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());
    ASH::TextureBarrier tb{};
    tb.texture = m_mask[frame].get();

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

void ShadowPass::destroy(Context& ctx)
{
    VkDevice dev = ctx.device();
    m_sbt.reset();
    m_mask.clear();
    m_sets.clear();
    if (m_pool)       vkDestroyDescriptorPool(dev, m_pool, nullptr);
    if (m_pipeline)   vkDestroyPipeline(dev, m_pipeline, nullptr);
    if (m_pipeLayout) vkDestroyPipelineLayout(dev, m_pipeLayout, nullptr);
    if (m_setLayout)  vkDestroyDescriptorSetLayout(dev, m_setLayout, nullptr);
    m_pool = VK_NULL_HANDLE; m_pipeline = VK_NULL_HANDLE;
    m_pipeLayout = VK_NULL_HANDLE; m_setLayout = VK_NULL_HANDLE;
    m_rgen = m_miss = m_hit = m_callable = {};
    m_tlas = VK_NULL_HANDLE; m_frames = 0;
}

bool ShadowPass::createSbt(Context& ctx)
{
    const auto& p = ctx.rtProps();
    const uint32_t hs = p.shaderGroupHandleSize;
    const uint32_t baseAlign = p.shaderGroupBaseAlignment;
    const uint32_t stride = alignUp(hs, p.shaderGroupHandleAlignment);
    const uint32_t groups = 4;

    std::vector<uint8_t> handles(size_t(groups) * hs);
    if (ctx.vkGetGroupHandles(ctx.device(), m_pipeline, 0, groups, handles.size(), handles.data()) != VK_SUCCESS)
        return false;

    const uint32_t rgenSize = alignUp(stride, baseAlign);
    const uint32_t missOff = rgenSize;
    const uint32_t missSize = alignUp(stride * 2, baseAlign);
    const uint32_t hitOff = missOff + missSize;
    const size_t total = size_t(hitOff) + alignUp(stride, baseAlign);

    m_sbt = ctx.createBuffer(total + baseAlign, ASH::BufferUsage::ShaderBindingTable,
        ASH::MemoryUsage::CpuToGpu, "RT sbt");
    const uint64_t base = m_sbt->getDeviceAddress();
    const uint64_t aligned = (base + baseAlign - 1) & ~uint64_t(baseAlign - 1);
    uint8_t* dst = static_cast<uint8_t*>(m_sbt->map()) + (aligned - base);
    std::memset(dst, 0, total);

    std::memcpy(dst, handles.data() + 0 * hs, hs);
    std::memcpy(dst + missOff, handles.data() + 1 * hs, hs);
    std::memcpy(dst + missOff + stride, handles.data() + 2 * hs, hs);
    std::memcpy(dst + hitOff, handles.data() + 3 * hs, hs);

    m_rgen = { aligned, rgenSize, rgenSize };
    m_miss = { aligned + missOff, stride, stride * 2 };
    m_hit  = { aligned + hitOff, stride, stride };
    return true;
}

}
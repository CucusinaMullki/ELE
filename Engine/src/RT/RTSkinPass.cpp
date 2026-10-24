// RTSkinPass.cpp
#include "RT/RTSkinPass.h"
#include "Vulkan/ShaderLoader.h"
#include "Vulkan/SkinnedVertex.h"
#include <vector>

static_assert(sizeof(SkinnedVertex) == 19 * sizeof(float),
    "RT skin.comp expects a tightly packed 19-float SkinnedVertex");

namespace RT
{

namespace
{
struct PushData { uint64_t src; uint64_t dst; uint32_t count; uint32_t pad; };
}

bool SkinPass::create(Context& ctx)
{
    VkDevice dev = ctx.device();

    VkDescriptorSetLayoutBinding b{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 1;
    lci.pBindings = &b;
    if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &m_setLayout) != VK_SUCCESS) return false;

    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipeLayout) != VK_SUCCESS) return false;

    std::vector<uint32_t> code = loadSpirvFile("skin.comp");
    if (code.empty()) return false;
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = code.size() * sizeof(uint32_t);
    smci.pCode = code.data();
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &smci, nullptr, &mod) != VK_SUCCESS) return false;

    VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
        VK_SHADER_STAGE_COMPUTE_BIT, mod, "main", nullptr};
    pci.layout = m_pipeLayout;
    const bool ok = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pci, nullptr, &m_pipeline) == VK_SUCCESS;
    vkDestroyShaderModule(dev, mod, nullptr);
    if (!ok) return false;

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxSets};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets = kMaxSets;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    return vkCreateDescriptorPool(dev, &dpci, nullptr, &m_pool) == VK_SUCCESS;
}

void SkinPass::destroy(Context& ctx)
{
    VkDevice dev = ctx.device();
    if (m_pool) vkDestroyDescriptorPool(dev, m_pool, nullptr);
    if (m_pipeline) vkDestroyPipeline(dev, m_pipeline, nullptr);
    if (m_pipeLayout) vkDestroyPipelineLayout(dev, m_pipeLayout, nullptr);
    if (m_setLayout) vkDestroyDescriptorSetLayout(dev, m_setLayout, nullptr);
    m_pool = VK_NULL_HANDLE; m_pipeline = VK_NULL_HANDLE;
    m_pipeLayout = VK_NULL_HANDLE; m_setLayout = VK_NULL_HANDLE;
}

VkDescriptorSet SkinPass::allocSet(Context& ctx, VkBuffer boneUbo)
{
    if (!m_pool || !boneUbo) return VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(ctx.device(), &ai, &set) != VK_SUCCESS) return VK_NULL_HANDLE;

    VkDescriptorBufferInfo bi{boneUbo, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo = &bi;
    vkUpdateDescriptorSets(ctx.device(), 1, &w, 0, nullptr);
    return set;
}

void SkinPass::freeSet(Context& ctx, VkDescriptorSet set)
{
    if (m_pool && set) vkFreeDescriptorSets(ctx.device(), m_pool, 1, &set);
}

void SkinPass::dispatch(VkCommandBuffer cmd, VkDescriptorSet set, uint64_t srcAddr, uint64_t dstAddr, uint32_t count)
{
    if (!m_pipeline || !set || count == 0) return;
    PushData pd{srcAddr, dstAddr, count, 0};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pd), &pd);
    vkCmdDispatch(cmd, (count + 63u) / 64u, 1, 1);
}

}
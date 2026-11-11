#include "RT/RTProbeUpdate.h"
#include "Vulkan/ShaderLoader.h"
#include <vector>

namespace RT
{

namespace
{
struct PushData { uint32_t frameIndex; uint32_t probeOffset; uint32_t rayCount; uint32_t pad; };
}

bool ProbeUpdatePass::create(Context& ctx)
{
    VkDevice dev = ctx.device();

    VkDescriptorSetLayoutBinding b[3]{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 3;
    lci.pBindings = b;
    if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &m_setLayout) != VK_SUCCESS) return false;

    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &m_pipeLayout) != VK_SUCCESS) return false;

    std::vector<uint32_t> code = loadSpirvFile("probe_update.comp");
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

    VkDescriptorPoolSize ps[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = ps;
    if (vkCreateDescriptorPool(dev, &dpci, nullptr, &m_pool) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;
    return vkAllocateDescriptorSets(dev, &ai, &m_set) == VK_SUCCESS;
}

void ProbeUpdatePass::destroy(Context& ctx)
{
    VkDevice dev = ctx.device();
    if (m_pool) vkDestroyDescriptorPool(dev, m_pool, nullptr);
    if (m_pipeline) vkDestroyPipeline(dev, m_pipeline, nullptr);
    if (m_pipeLayout) vkDestroyPipelineLayout(dev, m_pipeLayout, nullptr);
    if (m_setLayout) vkDestroyDescriptorSetLayout(dev, m_setLayout, nullptr);
    m_pool = VK_NULL_HANDLE; m_pipeline = VK_NULL_HANDLE;
    m_pipeLayout = VK_NULL_HANDLE; m_setLayout = VK_NULL_HANDLE;
    m_set = VK_NULL_HANDLE; m_bound = false;
}

bool ProbeUpdatePass::bind(Context& ctx, VkBuffer rays, VkBuffer info, VkBuffer data)
{
    if (!m_set || !rays || !info || !data) { m_bound = false; return false; }

    VkDescriptorBufferInfo bi[3]{
        {rays, 0, VK_WHOLE_SIZE},
        {info, 0, VK_WHOLE_SIZE},
        {data, 0, VK_WHOLE_SIZE},
    };
    const VkDescriptorType types[3] = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    VkWriteDescriptorSet w[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = m_set;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = types[i];
        w[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(ctx.device(), 3, w, 0, nullptr);
    m_bound = true;
    return true;
}

void ProbeUpdatePass::record(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t probeOffset,
                             uint32_t batch, uint32_t rayCount)
{
    if (!m_pipeline || !m_bound || batch == 0) return;

    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &mb, 0, nullptr, 0, nullptr);

    PushData pd{frameIndex, probeOffset, rayCount, 0};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &m_set, 0, nullptr);
    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pd), &pd);
    vkCmdDispatch(cmd, batch, 1, 1);

    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &mb, 0, nullptr, 0, nullptr);
}

}

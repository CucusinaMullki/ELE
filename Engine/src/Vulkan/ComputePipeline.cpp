#include "Vulkan/ComputePipeline.h"
#include "Vulkan/ShaderLoader.h"

void ComputePipeline::initialize(ASH::Device* device, const ComputePipelineConfig& config)
{
    m_device = device;

    ASH::DescriptorSetLayoutDesc layoutDesc = buildLayoutDesc(config.layoutType);
    m_descriptorSetLayout = device->createDescriptorSetLayout(layoutDesc);

    auto compCode = loadSpirvFile(config.computeShader + ".comp");

    ASH::ComputePipelineDesc desc{};
    desc.stage.stage = ASH::ShaderStage::Compute;
    desc.stage.spirvCode = compCode.data();
    desc.stage.spirvSize = compCode.size() * sizeof(uint32_t);

    desc.descriptorSetLayouts.push_back(m_descriptorSetLayout.get());

    if (config.pushConstantSize > 0)
    {
        ASH::PushConstantRange pcRange{};
        pcRange.stageFlags = config.pushStage;
        pcRange.offset = 0;
        pcRange.size = config.pushConstantSize;
        desc.pushConstantRanges.push_back(pcRange);
    }

    m_pipeline = device->createComputePipeline(desc);
}

void ComputePipeline::cleanup()
{
    m_pipeline.reset();
    m_descriptorSetLayout.reset();
}

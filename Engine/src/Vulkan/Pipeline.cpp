#include "Vulkan/Pipeline.h"
#include "Vulkan/Shader.h"
#include "Vulkan/Vertex.h"

void Pipeline::initialize(vk::Device device, vk::RenderPass renderPass, const std::string& shaderName, uint32_t frameCount)
{
    vk::DescriptorSetLayoutBinding uboBinding{};
    uboBinding.setBinding(0);
    uboBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uboBinding.setDescriptorCount(1);
    uboBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(uboBinding);
    m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutCreateInfo);

    vk::DescriptorPoolSize poolSize{};
    poolSize.setType(vk::DescriptorType::eUniformBuffer);
    poolSize.setDescriptorCount(frameCount);

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setPoolSizes(poolSize);
    poolInfo.setMaxSets(frameCount);
    m_descriptorPool = device.createDescriptorPoolUnique(poolInfo);

    std::vector<vk::DescriptorSetLayout> layouts(frameCount, m_descriptorSetLayout.get());
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(m_descriptorPool.get());
    allocInfo.setSetLayouts(layouts);
    m_descriptorSets = device.allocateDescriptorSets(allocInfo);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts(m_descriptorSetLayout.get());
    m_pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutInfo);

    Shader vertShader, fragShader;
    vertShader.load(device, shaderName, vk::ShaderStageFlagBits::eVertex);
    fragShader.load(device, shaderName, vk::ShaderStageFlagBits::eFragment);

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vertShader.getStageInfo(),
        fragShader.getStageInfo()
    };

    auto bindingDescription = Vertex::getBindingDescriptor();
    auto attributeDescription = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.setVertexBindingDescriptions(bindingDescription);
    vertexInput.setVertexAttributeDescriptions(attributeDescription);
    
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssembly.setPrimitiveRestartEnable(false);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(false);
    rasterizer.setRasterizerDiscardEnable(false);
    rasterizer.setPolygonMode(vk::PolygonMode::eFill);
    rasterizer.setLineWidth(1.0f);
    rasterizer.setCullMode(vk::CullModeFlagBits::eNone);
    rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterizer.setDepthBiasEnable(false);

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setSampleShadingEnable(false);
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    colorBlendAttachment.setBlendEnable(false);

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.setLogicOpEnable(false);
    colorBlending.setAttachments(colorBlendAttachment);

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1);
    viewportState.setScissorCount(1);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.setDepthTestEnable(true);
    depthStencil.setDepthWriteEnable(true);
    depthStencil.setDepthCompareOp(vk::CompareOp::eLess);
    depthStencil.setDepthBoundsTestEnable(false);
    depthStencil.setStencilTestEnable(false);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStages(shaderStages);
    pipelineInfo.setPVertexInputState(&vertexInput);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPDynamicState(&dynamicState);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterizer);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setPDepthStencilState(&depthStencil);
    pipelineInfo.setLayout(m_pipelineLayout.get());
    pipelineInfo.setRenderPass(renderPass);
    pipelineInfo.setSubpass(0);

    auto result = device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline!");

    m_graphicPipeline = std::move(result.value);
}

void Pipeline::cleanup()
{
    m_descriptorSets.clear();
    m_descriptorPool.reset();
    m_descriptorSetLayout.reset();
    m_graphicPipeline.reset();
    m_pipelineLayout.reset();
}

void Pipeline::updateDescriptorSets(vk::Device device, const std::vector<vk::DescriptorBufferInfo>& bufferInfos)
{
    for (size_t i = 0; i < bufferInfos.size(); i++)
    {
        vk::WriteDescriptorSet write{};
        write.setDstSet(m_descriptorSets[i]);
        write.setDstBinding(0);
        write.setDstArrayElement(0);
        write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        write.setDescriptorCount(1);
        write.setBufferInfo(bufferInfos[i]);

        device.updateDescriptorSets(write, nullptr);
    }
}
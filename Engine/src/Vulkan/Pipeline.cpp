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

    vk::DescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.setBinding(1);
    samplerBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    samplerBinding.setDescriptorCount(1);
    samplerBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { uboBinding, samplerBinding };

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(bindings);
    m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutCreateInfo);

    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].setType(vk::DescriptorType::eUniformBuffer);
    poolSizes[0].setDescriptorCount(frameCount);
    poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
    poolSizes[1].setDescriptorCount(frameCount);

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setPoolSizes(poolSizes);
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

void Pipeline::updateDescriptorSets(vk::Device device, const std::vector<vk::DescriptorBufferInfo>& bufferInfos, vk::ImageView textureImageView, vk::Sampler textureSampler)
{
    for (size_t i = 0; i < bufferInfos.size(); i++)
    {
        vk::DescriptorImageInfo imageInfo{};
        imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        imageInfo.setImageView(textureImageView);
        imageInfo.setSampler(textureSampler);

        std::array<vk::WriteDescriptorSet, 2> writes{};

        writes[0].setDstSet(m_descriptorSets[i]);
        writes[0].setDstBinding(0);
        writes[0].setDstArrayElement(0);
        writes[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
        writes[0].setDescriptorCount(1);
        writes[0].setBufferInfo(bufferInfos[i]);

        writes[1].setDstSet(m_descriptorSets[i]);
        writes[1].setDstBinding(1);
        writes[1].setDstArrayElement(0);
        writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        writes[1].setDescriptorCount(1);
        writes[1].setImageInfo(imageInfo);

        device.updateDescriptorSets(writes, nullptr);
    }
}
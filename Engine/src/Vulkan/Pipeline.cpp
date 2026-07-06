#include "Vulkan/Pipeline.h"
#include "Vulkan/Shader.h"
#include "Vulkan/Vertex.h"
#include <glm/glm.hpp>

void Pipeline::initialize(vk::Device device, vk::RenderPass renderPass, const std::string& shaderName, uint32_t frameCount)
{
    vk::DescriptorSetLayoutBinding uboBinding{};
    uboBinding.setBinding(0);
    uboBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uboBinding.setDescriptorCount(1);
    uboBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

    auto makeSamplerBinding = [](uint32_t bind) {
        vk::DescriptorSetLayoutBinding b{};
        b.setBinding(bind);
        b.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        b.setDescriptorCount(1);
        b.setStageFlags(vk::ShaderStageFlagBits::eFragment);
        return b;
    };

    std::array<vk::DescriptorSetLayoutBinding, 5> bindings = { 
        uboBinding, 
        makeSamplerBinding(1),
        makeSamplerBinding(2),
        makeSamplerBinding(3),
        makeSamplerBinding(4)
    };

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(bindings);
    m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutCreateInfo);

    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].setType(vk::DescriptorType::eUniformBuffer);
    poolSizes[0].setDescriptorCount(frameCount);
    poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
    poolSizes[1].setDescriptorCount(frameCount * 4);

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setPoolSizes(poolSizes);
    poolInfo.setMaxSets(frameCount);
    m_descriptorPool = device.createDescriptorPoolUnique(poolInfo);

    std::vector<vk::DescriptorSetLayout> layouts(frameCount, m_descriptorSetLayout.get());
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(m_descriptorPool.get());
    allocInfo.setSetLayouts(layouts);
    m_descriptorSets = device.allocateDescriptorSets(allocInfo);

    vk::PushConstantRange pushRange{};
    pushRange.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    pushRange.setOffset(0);
    pushRange.setSize(sizeof(glm::mat4) * 2);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts(m_descriptorSetLayout.get());
    pipelineLayoutInfo.setPushConstantRanges(pushRange);
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

void Pipeline::updateDescriptorSets(vk::Device device,
    const std::vector<vk::DescriptorBufferInfo>& bufferInfos,
    vk::ImageView albedo, vk::Sampler albedoSampler,
    vk::ImageView normal, vk::Sampler normalSampler,
    vk::ImageView metallic, vk::Sampler metallicSampler,
    vk::ImageView roughness, vk::Sampler roughnessSampler)
{
    for (size_t i = 0; i < bufferInfos.size(); i++)
    {
        auto makeImageInfo = [](vk::ImageView view, vk::Sampler sampler) {
            vk::DescriptorImageInfo info{};
            info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            info.setImageView(view);
            info.setSampler(sampler);
            return info;
        };

        auto albedoInfo    = makeImageInfo(albedo, albedoSampler);
        auto normalInfo    = makeImageInfo(normal, normalSampler);
        auto metallicInfo  = makeImageInfo(metallic, metallicSampler);
        auto roughnessInfo = makeImageInfo(roughness, roughnessSampler);

        std::array<vk::WriteDescriptorSet, 5> writes{};

        writes[0].setDstSet(m_descriptorSets[i]);
        writes[0].setDstBinding(0);
        writes[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
        writes[0].setDescriptorCount(1);
        writes[0].setBufferInfo(bufferInfos[i]);

        auto makeWrite = [&](uint32_t bind, vk::DescriptorImageInfo& info) {
            vk::WriteDescriptorSet w{};
            w.setDstSet(m_descriptorSets[i]);
            w.setDstBinding(bind);
            w.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            w.setDescriptorCount(1);
            w.setImageInfo(info);
            return w;
        };

        writes[1] = makeWrite(1, albedoInfo);
        writes[2] = makeWrite(2, normalInfo);
        writes[3] = makeWrite(3, metallicInfo);
        writes[4] = makeWrite(4, roughnessInfo);

        device.updateDescriptorSets(writes, nullptr);
    }
}
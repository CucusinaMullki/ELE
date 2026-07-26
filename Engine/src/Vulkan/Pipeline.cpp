#include "Vulkan/Pipeline.h"
#include "Vulkan/Shader.h"
#include "Vulkan/Vertex.h"
#include <glm/glm.hpp>
#include <array>

void Pipeline::initialize(vk::Device device, const PipelineConfig& config)
{
    if (config.postProcess)
    {
        vk::DescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.setBinding(0);
        samplerBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        samplerBinding.setDescriptorCount(1);
        samplerBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);
    
        vk::DescriptorSetLayoutBinding bloomBinding{};
        bloomBinding.setBinding(1);
        bloomBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        bloomBinding.setDescriptorCount(1);
        bloomBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);
    
        std::array<vk::DescriptorSetLayoutBinding, 2> postBindings = { samplerBinding, bloomBinding };
    
        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.setBindings(postBindings);
        m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutInfo);
    
        vk::DescriptorPoolSize poolSize{};
        poolSize.setType(vk::DescriptorType::eCombinedImageSampler);
        poolSize.setDescriptorCount(config.frameCount * 16);
    
        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.setPoolSizes(poolSize);
        poolInfo.setMaxSets(config.frameCount * 16);
        m_descriptorPool = device.createDescriptorPoolUnique(poolInfo);
    }
    else if (config.useDescriptors)
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

        vk::DescriptorSetLayoutBinding lightBinding{};
        lightBinding.setBinding(5);
        lightBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        lightBinding.setDescriptorCount(1);
        lightBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding shadowBinding{};
        shadowBinding.setBinding(6);
        shadowBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        shadowBinding.setDescriptorCount(1);
        shadowBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding pointShadowBinding{};
        pointShadowBinding.setBinding(7);
        pointShadowBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        pointShadowBinding.setDescriptorCount(1);
        pointShadowBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        std::array<vk::DescriptorSetLayoutBinding, 8> bindings = { 
            uboBinding, 
            makeSamplerBinding(1),
            makeSamplerBinding(2),
            makeSamplerBinding(3),
            makeSamplerBinding(4),
            lightBinding,
            shadowBinding,
            pointShadowBinding
        };

        vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.setBindings(bindings);
        m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutCreateInfo);

        std::array<vk::DescriptorPoolSize, 2> poolSizes{};

        poolSizes[0].setType(vk::DescriptorType::eUniformBuffer);
        poolSizes[0].setDescriptorCount(config.maxMaterials * config.frameCount * 2);
        poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
        poolSizes[1].setDescriptorCount(config.maxMaterials * config.frameCount * 5);

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.setPoolSizes(poolSizes);
        poolInfo.setMaxSets(config.maxMaterials * config.frameCount);
        m_descriptorPool = device.createDescriptorPoolUnique(poolInfo);
    } 
    else 
    {
        m_descriptorSetLayout.reset();
        m_descriptorPool.reset();
        m_descriptorSets.clear();
    }

    vk::PushConstantRange pushRange{};
    pushRange.setStageFlags(config.pushStage);
    pushRange.setOffset(0);
    pushRange.setSize(config.pushConstantSize);

    std::vector<vk::DescriptorSetLayout> setLayouts;
    if (config.useDescriptors || config.postProcess) setLayouts.push_back(m_descriptorSetLayout.get());

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts(setLayouts);
    pipelineLayoutInfo.setPushConstantRanges(pushRange);
    m_pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutInfo);

    Shader vertShader, fragShader;
    vertShader.load(device, config.vertexShader, vk::ShaderStageFlagBits::eVertex);

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vertShader.getStageInfo());

    if (!config.fragmentShader.empty())
    {
        fragShader.load(device, config.fragmentShader, vk::ShaderStageFlagBits::eFragment);
        shaderStages.push_back(fragShader.getStageInfo());
    }

    auto bindingDescription = Vertex::getBindingDescriptor();
    auto attributeDescription = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    if (config.useVertexInput)
    {
        vertexInput.setVertexBindingDescriptions(bindingDescription);
        vertexInput.setVertexAttributeDescriptions(attributeDescription);
    }
    
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssembly.setPrimitiveRestartEnable(false);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(config.depthClampEnable);
    rasterizer.setRasterizerDiscardEnable(false);
    rasterizer.setPolygonMode(vk::PolygonMode::eFill);
    rasterizer.setLineWidth(1.0f);
    rasterizer.setCullMode(config.cullMode);
    rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterizer.setDepthBiasEnable(config.depthBiasEnable);
    rasterizer.setDepthBiasConstantFactor(config.depthBiasConstant);
    rasterizer.setDepthBiasSlopeFactor(config.depthBiasSlope);

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setSampleShadingEnable(false);
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlending.setLogicOpEnable(false);
    if (config.colorAttachmentCount > 0)
    {
        colorBlendAttachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | 
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        colorBlendAttachment.setBlendEnable(config.blendEnable);
        colorBlendAttachment.setSrcColorBlendFactor(config.blendSrcFactor);
        colorBlendAttachment.setDstColorBlendFactor(config.blendDstFactor);
        colorBlendAttachment.setColorBlendOp(config.blendOp);
        colorBlendAttachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
        colorBlendAttachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
        colorBlendAttachment.setAlphaBlendOp(vk::BlendOp::eAdd);
        colorBlending.setAttachments({colorBlendAttachment});
    }
    else
    {
        colorBlending.setAttachmentCount(0);
    }

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
    depthStencil.setDepthTestEnable(config.depthTest);
    depthStencil.setDepthWriteEnable(config.depthWrite);
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
    pipelineInfo.setRenderPass(config.renderPass);
    pipelineInfo.setSubpass(0);

    auto result = device.createGraphicsPipelineUnique(config.cache, pipelineInfo);
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
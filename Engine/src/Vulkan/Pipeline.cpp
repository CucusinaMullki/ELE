#include "Vulkan/Pipeline.h"
#include "Vulkan/Shader.h"
#include "Vulkan/Vertex.h"
#include <glm/glm.hpp>
#include "Core/Log.h"
#include <array>

void Pipeline::initialize(vk::Device device, const PipelineConfig& config)
{
    if (config.fogLayout)
    {
        vk::DescriptorSetLayoutBinding depthBinding{};
        depthBinding.setBinding(0);
        depthBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        depthBinding.setDescriptorCount(1);
        depthBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.setBinding(1);
        cameraBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        cameraBinding.setDescriptorCount(1);
        cameraBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding shadowBinding{};
        shadowBinding.setBinding(2);
        shadowBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        shadowBinding.setDescriptorCount(1);
        shadowBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding lightBinding{};
        lightBinding.setBinding(3);
        lightBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        lightBinding.setDescriptorCount(1);
        lightBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        std::array<vk::DescriptorSetLayoutBinding, 4> bindings = 
            { depthBinding, cameraBinding, shadowBinding, lightBinding };

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.setBindings(bindings);
        m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutInfo);

        std::array<vk::DescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].setType(vk::DescriptorType::eCombinedImageSampler);
        poolSizes[0].setDescriptorCount(config.frameCount * 4);
        poolSizes[1].setType(vk::DescriptorType::eUniformBuffer);
        poolSizes[1].setDescriptorCount(config.frameCount * 4);

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.setPoolSizes(poolSizes);
        poolInfo.setMaxSets(config.frameCount * 2);
        m_descriptorPool = device.createDescriptorPoolUnique(poolInfo);
    }
    else if (config.decalLayout)
    {
        vk::DescriptorSetLayoutBinding decalTexBinding{};
        decalTexBinding.setBinding(0);
        decalTexBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        decalTexBinding.setDescriptorCount(1);
        decalTexBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding depthTexBinding{};
        depthTexBinding.setBinding(1);
        depthTexBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        depthTexBinding.setDescriptorCount(1);
        depthTexBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);
        
        vk::DescriptorSetLayoutBinding cameraBinding{};
        cameraBinding.setBinding(2);
        cameraBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        cameraBinding.setDescriptorCount(1);
        cameraBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding lightBinding{};
        lightBinding.setBinding(3);
        lightBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        lightBinding.setDescriptorCount(1);
        lightBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding opacityTexBinding{};
        opacityTexBinding.setBinding(4).setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding normalTexBinding{};
        normalTexBinding.setBinding(5).setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment);

        vk::DescriptorSetLayoutBinding mrTexBinding{};
        mrTexBinding.setBinding(6).setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment);

        std::array<vk::DescriptorSetLayoutBinding, 7> bindings = { decalTexBinding, depthTexBinding, cameraBinding, lightBinding, opacityTexBinding, normalTexBinding, mrTexBinding };

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.setBindings(bindings);
        m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutInfo);
        LOG("decalLayout created, layout ptr=" << (void*)(VkDescriptorSetLayout)m_descriptorSetLayout.get());

        std::array<vk::DescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].setType(vk::DescriptorType::eCombinedImageSampler);
        poolSizes[0].setDescriptorCount(config.frameCount * 32 * 2);
        poolSizes[1].setType(vk::DescriptorType::eUniformBuffer);
        poolSizes[1].setDescriptorCount(config.frameCount * 32);

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.setPoolSizes(poolSizes);
        poolInfo.setMaxSets(config.frameCount * 32);
        m_descriptorPool = device.createDescriptorPoolUnique(poolInfo);
    }
    else if (config.postProcess)
    {
        std::array<vk::DescriptorSetLayoutBinding, 3> postBindings{};
        for (uint32_t b = 0; b < 3; b++)
        {
            postBindings[b].setBinding(b);
            postBindings[b].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            postBindings[b].setDescriptorCount(1);
            postBindings[b].setStageFlags(vk::ShaderStageFlagBits::eFragment);
        }

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.setBindings(postBindings);
        m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutInfo);

        vk::DescriptorPoolSize poolSize{};
        poolSize.setType(vk::DescriptorType::eCombinedImageSampler);
        poolSize.setDescriptorCount(config.frameCount * 256 * 3);

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.setPoolSizes(poolSize);
        poolInfo.setMaxSets(config.frameCount * 256);
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

        vk::DescriptorSetLayoutBinding lightmapBinding{};
        lightmapBinding.setBinding(8);
        lightmapBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        lightmapBinding.setDescriptorCount(1);
        lightmapBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        std::array<vk::DescriptorSetLayoutBinding, 9> bindings = {
            uboBinding,
            makeSamplerBinding(1),
            makeSamplerBinding(2),
            makeSamplerBinding(3),
            makeSamplerBinding(4),
            lightBinding,
            shadowBinding,
            pointShadowBinding,
            lightmapBinding
        };

        vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.setBindings(bindings);
        m_descriptorSetLayout = device.createDescriptorSetLayoutUnique(layoutCreateInfo);

        std::array<vk::DescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].setType(vk::DescriptorType::eUniformBuffer);
        poolSizes[0].setDescriptorCount(config.maxMaterials * config.frameCount * 2);
        poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
        poolSizes[1].setDescriptorCount(config.maxMaterials * config.frameCount * 7);

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
    if (config.useDescriptors || config.postProcess || config.decalLayout || config.fogLayout)
        setLayouts.push_back(m_descriptorSetLayout.get());

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
    colorBlending.setLogicOpEnable(false);

    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;

    if (config.colorAttachmentCount > 0)
    {
        colorBlendAttachments.resize(config.colorAttachmentCount);

        for (auto& cba : colorBlendAttachments)
        {
            cba.setColorWriteMask(
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
            cba.setBlendEnable(config.blendEnable);
            cba.setSrcColorBlendFactor(config.blendSrcFactor);
            cba.setDstColorBlendFactor(config.blendDstFactor);
            cba.setColorBlendOp(config.blendOp);
            cba.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
            cba.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
            cba.setAlphaBlendOp(vk::BlendOp::eAdd);
        }

        colorBlending.setAttachments(colorBlendAttachments);
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
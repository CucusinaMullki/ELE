#include "Vulkan/ShadowPipeline.h"
#include "Vulkan/Vertex.h"
#include "Core/Log.h"
#include <stdexcept>

void ShadowPipeline::initialize(vk::Device device, vk::RenderPass renderPass, vk::PipelineCache cache)
{
    m_vertexShader.load(device, "shadow/vertex", vk::ShaderStageFlagBits::eVertex);
    auto vertStage = m_vertexShader.getStageInfo();

    auto bindingDesc = Vertex::getBindingDescriptor();
    
    vk::VertexInputAttributeDescription posAttr{};
    posAttr.setBinding(0);
    posAttr.setLocation(0);
    posAttr.setFormat(vk::Format::eR32G32B32Sfloat);
    posAttr.setOffset(offsetof(Vertex, pos));

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.setVertexBindingDescriptionCount(1);
    vertexInput.setPVertexBindingDescriptions(&bindingDesc);
    vertexInput.setVertexAttributeDescriptionCount(1);
    vertexInput.setPVertexAttributeDescriptions(&posAttr);

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1);
    viewportState.setScissorCount(1);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(false);
    rasterizer.setRasterizerDiscardEnable(false);
    rasterizer.setPolygonMode(vk::PolygonMode::eFill);
    rasterizer.setLineWidth(1.0f);
    rasterizer.setCullMode(vk::CullModeFlagBits::eFront);
    rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterizer.setDepthBiasEnable(true);
    rasterizer.setDepthBiasConstantFactor(1.25f);
    rasterizer.setDepthBiasClamp(0.0f);
    rasterizer.setDepthBiasSlopeFactor(2.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setSampleShadingEnable(false);
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.setDepthTestEnable(true);
    depthStencil.setDepthWriteEnable(true);
    depthStencil.setDepthCompareOp(vk::CompareOp::eLess);
    depthStencil.setDepthBoundsTestEnable(false);
    depthStencil.setStencilTestEnable(false);

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStateCount(2);
    dynamicState.setPDynamicStates(dynamicStates.data());

    vk::PushConstantRange pushRange{};
    pushRange.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    pushRange.setOffset(0);
    pushRange.setSize(sizeof(glm::mat4));

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.setPushConstantRangeCount(1);
    layoutInfo.setPPushConstantRanges(&pushRange);
    layoutInfo.setSetLayoutCount(0);
    m_layout = device.createPipelineLayoutUnique(layoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStageCount(1);
    pipelineInfo.setPStages(&vertStage);
    pipelineInfo.setPVertexInputState(&vertexInput);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterizer);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPDepthStencilState(&depthStencil);
    pipelineInfo.setPColorBlendState(nullptr);
    pipelineInfo.setPDynamicState(&dynamicState);
    pipelineInfo.setLayout(m_layout.get());
    pipelineInfo.setRenderPass(renderPass);
    pipelineInfo.setSubpass(0);

    auto result = device.createGraphicsPipelineUnique(cache, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create shadow pipeline!");
    m_pipeline = std::move(result.value);

    LOG("Shadow pipeline created");
}

void ShadowPipeline::cleanup()
{
    m_pipeline.reset();
    m_layout.reset();
    m_vertexShader.cleanup();
}
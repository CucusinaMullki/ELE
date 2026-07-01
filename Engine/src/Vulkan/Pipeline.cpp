#include "Vulkan/Pipeline.h"

void Pipeline::initialize(vk::Device device, vk::Extent2D swapchainExtent, vk::RenderPass renderPass)
{
    auto vertCode = readFile("shaders/shader.vert.spv");
    auto fragCode = readFile("shaders/shader.frag.spv");

    auto vertModule = createModule(device, vertCode);
    auto fragModule = createModule(device, fragCode);

    vk::PipelineShaderStageCreateInfo vertStage{};
    vertStage.setStage(vk::ShaderStageFlagBits::eVertex);
    vertStage.setModule(vertModule.get());
    vertStage.setPName("main");

    vk::PipelineShaderStageCreateInfo fragStage{};
    fragStage.setStage(vk::ShaderStageFlagBits::eFragment);
    fragStage.setModule(fragModule.get());
    fragStage.setPName("main");

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = { vertStage, fragStage };

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssembly.setPrimitiveRestartEnable(false);


    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(false);
    rasterizer.setRasterizerDiscardEnable(false);
    rasterizer.setPolygonMode(vk::PolygonMode::eFill);
    rasterizer.setLineWidth(1.0f);
    rasterizer.setCullMode(vk::CullModeFlagBits::eBack);
    rasterizer.setFrontFace(vk::FrontFace::eClockwise);
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

    vk::PipelineLayoutCreateInfo layoutInfo{};
    m_pipelineLayout = device.createPipelineLayoutUnique(layoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStages(shaderStages);
    pipelineInfo.setPVertexInputState(&vertexInput);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPDynamicState(&dynamicState);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterizer);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setLayout(m_pipelineLayout.get());
    pipelineInfo.setRenderPass(renderPass);
    pipelineInfo.setSubpass(0);


    auto result = device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    m_graphicPipeline = std::move(result.value);
}

std::vector<char> Pipeline::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

vk::UniqueShaderModule Pipeline::createModule(vk::Device device, const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.setCodeSize(code.size());
    createInfo.setPCode(reinterpret_cast<const uint32_t*>(code.data()));

    return device.createShaderModuleUnique(createInfo);
}

void Pipeline::cleanup()
{
    m_graphicPipeline.reset();
    m_pipelineLayout.reset();
}
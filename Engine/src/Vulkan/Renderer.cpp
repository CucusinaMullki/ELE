#include "Vulkan/Renderer.h"
#include "Components/SkeletalMeshComponent.h"
#include "vulkan/VulkanDevice.h"
#include "vulkan/VulkanRenderPass.h"
#include "vulkan/VulkanCommandBuffer.h"
#include "Vulkan/PushConstant.h"
#include <vector>
#include "Core/Log.h"

namespace
{
    float haltonSeq(uint32_t index, uint32_t base)
    {
        float result = 0.0f;
        float f = 1.0f;
        while (index > 0)
        {
            f /= base;
            result += f * (index % base);
            index /= base;
        }
        return result;
    }
}

glm::vec2 Renderer::getJitter() const
{
    uint32_t idx = (m_frameIndex % 16) + 1;
    float x = haltonSeq(idx, 2) - 0.5f;
    float y = haltonSeq(idx, 3) - 0.5f;
    return glm::vec2(x, y);
}

void Renderer::initImGui(GLFWwindow* window)
{
    auto* vulkanDevice = static_cast<ASH::vulkan::VulkanDevice*>(m_context.getDevice());

    std::array<vk::DescriptorPoolSize, 11> poolSizes = {{
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
    }};
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    poolInfo.setMaxSets(1000);
    poolInfo.setPoolSizes(poolSizes);
    m_imguiDescriptorPool = vk::Device(vulkanDevice->getHandle()).createDescriptorPoolUnique(poolInfo);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    uint32_t swapchainImageCount = m_swapchain.getImageCount();

    auto* vulkanRenderPass = static_cast<ASH::vulkan::VulkanRenderPass*>(m_renderPass.getRenderPass());

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = vulkanDevice->getInstance();
    initInfo.PhysicalDevice = vulkanDevice->getPhysicalDevice();
    initInfo.Device = vulkanDevice->getHandle();
    initInfo.QueueFamily = vulkanDevice->getGraphicsQueueFamily();
    initInfo.Queue = vulkanDevice->getGraphicsQueue();
    initInfo.DescriptorPool = m_imguiDescriptorPool.get();
    initInfo.RenderPass = vulkanRenderPass->getHandle();
    initInfo.MinImageCount = swapchainImageCount;
    initInfo.ImageCount = swapchainImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void Renderer::beginImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Renderer::renderImGui(ASH::CommandBuffer* commandBuffer)
{
    auto* vulkanCmd = static_cast<ASH::vulkan::VulkanCommandBuffer*>(commandBuffer);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vulkanCmd->getHandle());
}

void Renderer::cleanupImGui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Renderer::initialize(GLFWwindow* window)
{
    m_window = window;

    m_context.initialize(m_window);
    m_swapchain.initialize(m_context, m_window);
    LOG("swapchain extent: " << m_swapchain.getExtent().width << "x" << m_swapchain.getExtent().height << "y");

    ASH::Device* device = m_context.getDevice();

    m_depthBuffer.initialize(device, m_swapchain.getExtent());
    LOG("depth buffer created with extent from swapchain");

    m_renderPass.initialize(device, m_swapchain.getImageFormat(), m_depthBuffer.getFormat());

    setAAMethod(AAMethod::None);

    m_hdrTarget.initialize(device, m_swapchain.getExtent());
    m_velocityTarget.initialize(device, m_swapchain.getExtent());
    m_hdrRenderPass.initialize(device, m_hdrTarget.getColorFormat(), m_velocityTarget.getColorFormat(), m_depthBuffer.getFormat());

    ASH::FramebufferDesc hdrFbDesc{};
    hdrFbDesc.renderPass = m_hdrRenderPass.getRenderPass();
    hdrFbDesc.colorAttachments = { m_hdrTarget.getColorTexture(), m_velocityTarget.getColorTexture() };
    hdrFbDesc.depthStencilAttachment = m_depthBuffer.getTexture();
    hdrFbDesc.extent = m_swapchain.getExtent();
    hdrFbDesc.layers = 1;
    m_hdrFramebuffer = device->createFramebuffer(hdrFbDesc);

    m_hdrDecalRenderPass.initialize(device, m_hdrTarget.getColorFormat(), ASH::LoadOp::Load);

    ASH::FramebufferDesc hdrDecalFbDesc{};
    hdrDecalFbDesc.renderPass = m_hdrDecalRenderPass.getRenderPass();
    hdrDecalFbDesc.colorAttachments = { m_hdrTarget.getColorTexture() };
    hdrDecalFbDesc.extent = m_swapchain.getExtent();
    hdrDecalFbDesc.layers = 1;
    m_hdrDecalFramebuffer = device->createFramebuffer(hdrDecalFbDesc);

    ASH::Extent2D bloomExtent = m_swapchain.getExtent();
    m_brightTarget.initialize(device, bloomExtent);
    m_postRenderPass.initialize(device, m_brightTarget.getColorFormat(), ASH::LoadOp::Clear);

    m_historyBuffer.initialize(device, m_swapchain.getExtent());

    for (uint32_t i = 0; i < 2; i++)
    {
        ASH::FramebufferDesc fbDesc{};
        fbDesc.renderPass = m_postRenderPass.getRenderPass();
        fbDesc.colorAttachments = { m_historyBuffer.getTexture(i) };
        fbDesc.extent = m_swapchain.getExtent();
        fbDesc.layers = 1;
        m_taaFramebuffers[i] = device->createFramebuffer(fbDesc);
    }

    PipelineConfig taaConfig{};
    taaConfig.vertexShader = "fullscreen";
    taaConfig.fragmentShader = "taa";
    taaConfig.renderPass = m_postRenderPass.getRenderPass();
    taaConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    taaConfig.layoutType = LayoutType::PostProcess;
    taaConfig.useVertexInput = false;
    taaConfig.depthTest = false;
    taaConfig.depthWrite = false;
    taaConfig.cullMode = ASH::CullMode::None;
    taaConfig.pushStage = ASH::ShaderStage::Fragment;
    taaConfig.pushConstantSize = sizeof(float);
    m_taaPipeline.initialize(device, taaConfig);


    for (uint32_t slot = 0; slot < 2; slot++)
    {
        m_taaDescriptorSets[slot].clear();
        m_taaDescriptorSets[slot].resize(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            m_taaDescriptorSets[slot][i] = m_taaPipeline.createDescriptorSet();
    }

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        uint32_t prevSlot = 1 - slot;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            ASH::DescriptorImageInfo hdrInfo{ m_hdrTarget.getColorTexture(), m_hdrTarget.getSampler() };
            ASH::DescriptorImageInfo velInfo{ m_velocityTarget.getColorTexture(), m_velocityTarget.getSampler() };
            ASH::DescriptorImageInfo histInfo{ m_historyBuffer.getTexture(prevSlot), m_historyBuffer.getSampler() };

            std::array<ASH::DescriptorWrite, 3> writes{};
            writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &hdrInfo };
            writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &velInfo };
            writes[2] = { 2, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &histInfo };

            m_taaDescriptorSets[slot][i]->update(writes.data(), (uint32_t)writes.size());
        }
    }


    ASH::FramebufferDesc brightFbDesc{};
    brightFbDesc.renderPass = m_postRenderPass.getRenderPass();
    brightFbDesc.colorAttachments = { m_brightTarget.getColorTexture() };
    brightFbDesc.extent = bloomExtent;
    brightFbDesc.layers = 1;
    m_brightFramebuffer = device->createFramebuffer(brightFbDesc);

    PipelineConfig brightConfig{};
    brightConfig.vertexShader = "fullscreen";
    brightConfig.fragmentShader = "brightpass";
    brightConfig.renderPass = m_postRenderPass.getRenderPass();
    brightConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    brightConfig.layoutType = LayoutType::PostProcess;
    brightConfig.useVertexInput = false;
    brightConfig.depthTest = false;
    brightConfig.depthWrite = false;
    brightConfig.cullMode = ASH::CullMode::None;
    brightConfig.pushStage = ASH::ShaderStage::Fragment;
    brightConfig.pushConstantSize = sizeof(float);
    m_brightPipeline.initialize(device, brightConfig);

    auto allocDescSets = [&](Pipeline& pipe, uint32_t count) {
        std::vector<std::unique_ptr<ASH::DescriptorSet>> sets;
        sets.resize(count);
        for (uint32_t i = 0; i < count; i++)
            sets[i] = pipe.createDescriptorSet();
        return sets;
    };

    auto writeDescSet = [&](ASH::DescriptorSet* set, ASH::Texture* texture, ASH::Sampler* sampler) {
        ASH::DescriptorImageInfo info{ texture, sampler };
        ASH::DescriptorWrite write{ 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &info };
        set->update(&write, 1);
    };

    m_brightDescriptorSets[0] = allocDescSets(m_brightPipeline, MAX_FRAMES_IN_FLIGHT);
    m_brightDescriptorSets[1] = allocDescSets(m_brightPipeline, MAX_FRAMES_IN_FLIGHT);

    for (uint32_t slot = 0; slot < 2; slot++)
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_brightDescriptorSets[slot][i].get(), m_historyBuffer.getTexture(slot), m_historyBuffer.getSampler());

    m_brightDescriptorSetsRaw = allocDescSets(m_brightPipeline, MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_brightDescriptorSetsRaw[i].get(), m_hdrTarget.getColorTexture(), m_hdrTarget.getSampler());
    m_bloomChain.initialize(device, bloomExtent);

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        ASH::FramebufferDesc fbDesc{};
        fbDesc.renderPass = m_postRenderPass.getRenderPass();
        fbDesc.colorAttachments = { m_bloomChain.getMipTexture(level) };
        fbDesc.extent = m_bloomChain.getMipExtent(level);
        fbDesc.layers = 1;
        m_bloomFramebuffers[level] = device->createFramebuffer(fbDesc);
    }

    PipelineConfig downsampleConfig{};
    downsampleConfig.vertexShader = "fullscreen";
    downsampleConfig.fragmentShader = "downsample";
    downsampleConfig.renderPass = m_postRenderPass.getRenderPass();
    downsampleConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    downsampleConfig.layoutType = LayoutType::PostProcess;
    downsampleConfig.useVertexInput = false;
    downsampleConfig.depthTest = false;
    downsampleConfig.depthWrite = false;
    downsampleConfig.cullMode = ASH::CullMode::None;
    downsampleConfig.pushStage = ASH::ShaderStage::Fragment;
    downsampleConfig.pushConstantSize = sizeof(glm::vec2);
    m_downsamplePipeline.initialize(device, downsampleConfig);

    PipelineConfig upsampleConfig = downsampleConfig;
    upsampleConfig.fragmentShader = "upsample";
    upsampleConfig.pushConstantSize = sizeof(float);
    upsampleConfig.blendEnable = true;
    upsampleConfig.blendSrcFactor = ASH::BlendFactor::One;
    upsampleConfig.blendDstFactor = ASH::BlendFactor::One;
    upsampleConfig.blendOp = ASH::BlendOp::Add;
    m_upsamplePipeline.initialize(device, upsampleConfig);

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        m_downsampleDescriptorSets[level] = allocDescSets(m_downsamplePipeline, MAX_FRAMES_IN_FLIGHT);
        ASH::Texture* srcTex = (level == 0) ? m_brightTarget.getColorTexture() : m_bloomChain.getMipTexture(level - 1);
        ASH::Sampler* srcSampler = (level == 0) ? m_brightTarget.getSampler() : m_bloomChain.getSampler();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_downsampleDescriptorSets[level][i].get(), srcTex, srcSampler);
    }

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT - 1; level++)
    {
        m_upsampleDescriptorSets[level] = allocDescSets(m_upsamplePipeline, MAX_FRAMES_IN_FLIGHT);
        ASH::Texture* srcTex = m_bloomChain.getMipTexture(level + 1);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_upsampleDescriptorSets[level][i].get(), srcTex, m_bloomChain.getSampler());
    }

    m_godRayMaskTarget.initialize(device, m_swapchain.getExtent());
    m_godRayBlurTarget.initialize(device, m_swapchain.getExtent());

    ASH::FramebufferDesc maskFbDesc{};
    maskFbDesc.renderPass = m_postRenderPass.getRenderPass();
    maskFbDesc.colorAttachments = { m_godRayMaskTarget.getColorTexture() };
    maskFbDesc.extent = bloomExtent;
    maskFbDesc.layers = 1;
    m_godRayMaskFramebuffer = device->createFramebuffer(maskFbDesc);

    ASH::FramebufferDesc blurFbDesc{};
    blurFbDesc.renderPass = m_postRenderPass.getRenderPass();
    blurFbDesc.colorAttachments = { m_godRayBlurTarget.getColorTexture() };
    blurFbDesc.extent = bloomExtent;
    blurFbDesc.layers = 1;
    m_godRayBlurFramebuffer = device->createFramebuffer(blurFbDesc);

    PipelineConfig godRayMaskConfig{};
    godRayMaskConfig.vertexShader = "fullscreen";
    godRayMaskConfig.fragmentShader = "godray_mask";
    godRayMaskConfig.renderPass = m_postRenderPass.getRenderPass();
    godRayMaskConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    godRayMaskConfig.layoutType = LayoutType::PostProcess;
    godRayMaskConfig.useVertexInput = false;
    godRayMaskConfig.depthTest = false;
    godRayMaskConfig.depthWrite = false;
    godRayMaskConfig.cullMode = ASH::CullMode::None;
    godRayMaskConfig.pushStage = ASH::ShaderStage::Fragment;
    godRayMaskConfig.pushConstantSize = sizeof(glm::mat4) + sizeof(glm::vec4) * 2;
    m_godRayMaskPipeline.initialize(device, godRayMaskConfig);

    PipelineConfig godRayBlurConfig{};
    godRayBlurConfig.vertexShader = "fullscreen";
    godRayBlurConfig.fragmentShader = "godray_blur";
    godRayBlurConfig.renderPass = m_postRenderPass.getRenderPass();
    godRayBlurConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    godRayBlurConfig.layoutType = LayoutType::PostProcess;
    godRayBlurConfig.useVertexInput = false;
    godRayBlurConfig.depthTest = false;
    godRayBlurConfig.depthWrite = false;
    godRayBlurConfig.cullMode = ASH::CullMode::None;
    godRayBlurConfig.pushStage = ASH::ShaderStage::Fragment;
    godRayBlurConfig.pushConstantSize = sizeof(glm::vec4) * 2;
    m_godRayBlurPipeline.initialize(device, godRayBlurConfig);

    PipelineConfig godRayCompositeConfig{};
    godRayCompositeConfig.vertexShader = "fullscreen";
    godRayCompositeConfig.fragmentShader = "godray_composite";
    godRayCompositeConfig.renderPass = m_hdrDecalRenderPass.getRenderPass();
    godRayCompositeConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    godRayCompositeConfig.layoutType = LayoutType::PostProcess;
    godRayCompositeConfig.useVertexInput = false;
    godRayCompositeConfig.colorAttachmentCount = 1;
    godRayCompositeConfig.depthTest = false;
    godRayCompositeConfig.depthWrite = false;
    godRayCompositeConfig.cullMode = ASH::CullMode::None;
    godRayCompositeConfig.blendEnable = true;
    godRayCompositeConfig.blendSrcFactor = ASH::BlendFactor::One;
    godRayCompositeConfig.blendDstFactor = ASH::BlendFactor::One;
    godRayCompositeConfig.blendOp = ASH::BlendOp::Add;
    godRayCompositeConfig.pushStage = ASH::ShaderStage::Fragment;
    godRayCompositeConfig.pushConstantSize = sizeof(float);
    m_godRayCompositePipeline.initialize(device, godRayCompositeConfig);

    m_godRayMaskDescriptorSets = allocDescSets(m_godRayMaskPipeline, MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayMaskDescriptorSets[i].get(), m_depthBuffer.getTexture(), m_depthBuffer.getSampler());

    m_godRayBlurDescriptorSets = allocDescSets(m_godRayBlurPipeline, MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayBlurDescriptorSets[i].get(), m_godRayMaskTarget.getColorTexture(), m_godRayMaskTarget.getSampler());

    m_godRayCompositeDescriptorSets = allocDescSets(m_godRayCompositePipeline, MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayCompositeDescriptorSets[i].get(), m_godRayBlurTarget.getColorTexture(), m_godRayBlurTarget.getSampler());

    m_postRenderPassLoad.initialize(device, m_brightTarget.getColorFormat(), ASH::LoadOp::Load);
    m_ldrTarget.initialize(device, m_swapchain.getExtent());
    m_ldrRenderPass.initialize(device, m_ldrTarget.getColorFormat(), ASH::LoadOp::Clear);

    ASH::FramebufferDesc ldrFbDesc{};
    ldrFbDesc.renderPass = m_ldrRenderPass.getRenderPass();
    ldrFbDesc.colorAttachments = { m_ldrTarget.getColorTexture() };
    ldrFbDesc.extent = m_swapchain.getExtent();
    ldrFbDesc.layers = 1;
    m_ldrFramebuffer = device->createFramebuffer(ldrFbDesc);

    PipelineConfig postConfig{};
    postConfig.vertexShader = "fullscreen";
    postConfig.fragmentShader = "tonemap";
    postConfig.renderPass = m_ldrRenderPass.getRenderPass();
    postConfig.useVertexInput = false;
    postConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    postConfig.pushStage = ASH::ShaderStage::Fragment;
    postConfig.pushConstantSize = sizeof(float) * 2;
    postConfig.layoutType = LayoutType::PostProcess;
    postConfig.colorAttachmentCount = 1;
    postConfig.depthTest = false;
    postConfig.depthWrite = false;
    postConfig.cullMode = ASH::CullMode::None;
    m_postPipeline.initialize(device, postConfig);

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        m_postDescriptorSets[slot] = allocDescSets(m_postPipeline, MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            ASH::DescriptorImageInfo hdrInfo{ m_historyBuffer.getTexture(slot), m_historyBuffer.getSampler() };
            ASH::DescriptorImageInfo bloomInfo{ m_bloomChain.getMipTexture(0), m_bloomChain.getSampler() };

            std::array<ASH::DescriptorWrite, 2> writes{};
            writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &hdrInfo };
            writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &bloomInfo };

            m_postDescriptorSets[slot][i]->update(writes.data(), (uint32_t)writes.size());
        }
    }

    {
        m_postDescriptorSetsRaw = allocDescSets(m_postPipeline, MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            ASH::DescriptorImageInfo hdrInfo{ m_hdrTarget.getColorTexture(), m_hdrTarget.getSampler() };
            ASH::DescriptorImageInfo bloomInfo{ m_bloomChain.getMipTexture(0), m_bloomChain.getSampler() };

            std::array<ASH::DescriptorWrite, 2> writes{};
            writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &hdrInfo };
            writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &bloomInfo };

            m_postDescriptorSetsRaw[i]->update(writes.data(), (uint32_t)writes.size());
        }
    }

    m_fxaa.initializeWithRenderPass(device, m_renderPass.getRenderPass(), MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        m_fxaa.updateDescriptorSet(i, m_ldrTarget.getColorTexture(), m_ldrTarget.getSampler());

    PipelineConfig pbrConfig{};
    pbrConfig.vertexShader = "pbr";
    pbrConfig.fragmentShader = "pbr";
    pbrConfig.renderPass = m_hdrRenderPass.getRenderPass();
    pbrConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    pbrConfig.layoutType = LayoutType::Default;
    pbrConfig.colorAttachmentCount = 2;
    pbrConfig.cullMode = ASH::CullMode::Back;
    pbrConfig.pushConstantSize = sizeof(glm::mat4) * 3;
    m_pipeline.initialize(device, pbrConfig);

    PipelineConfig skinnedConfig{};
    skinnedConfig.vertexShader = "skinned";
    skinnedConfig.fragmentShader = "skinned";
    skinnedConfig.renderPass = m_hdrRenderPass.getRenderPass();
    skinnedConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    skinnedConfig.layoutType = LayoutType::Skinned;
    skinnedConfig.colorAttachmentCount = 2;
    skinnedConfig.cullMode = ASH::CullMode::Back;
    skinnedConfig.pushConstantSize = sizeof(glm::mat4) * 3;
    m_skinnedPipeline.initialize(device, skinnedConfig);

    PipelineConfig decalConfig{};
    decalConfig.vertexShader = "decal_box";
    decalConfig.fragmentShader = "decal_box";
    decalConfig.renderPass = m_hdrDecalRenderPass.getRenderPass();
    decalConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    decalConfig.layoutType = LayoutType::Decal;
    decalConfig.colorAttachmentCount = 1;
    decalConfig.pushConstantSize = sizeof(glm::mat4) * 2 + sizeof(glm::vec4);
    decalConfig.pushStage = ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment;
    decalConfig.blendEnable = true;
    decalConfig.blendSrcFactor = ASH::BlendFactor::SrcAlpha;
    decalConfig.blendDstFactor = ASH::BlendFactor::OneMinusSrcAlpha;
    decalConfig.blendOp = ASH::BlendOp::Add;
    decalConfig.cullMode = ASH::CullMode::Front;
    decalConfig.depthTest = false;
    decalConfig.depthWrite = false;
    decalConfig.useVertexInput = true;
    m_decalBox.initialize(device);
    m_decalCameraBuffer.initialize(device, MAX_FRAMES_IN_FLIGHT);
    m_decalPipeline.initialize(device, decalConfig);

    m_lightBuffer.initialize(device, MAX_FRAMES_IN_FLIGHT);

    m_commandManager.initialize(device, MAX_FRAMES_IN_FLIGHT);

    m_uniformBuffer.initialize(device, MAX_FRAMES_IN_FLIGHT);
    m_framebuffers.initialize(device, m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getTexture());

    m_shadowMap.initialize(device, 2048);
    m_shadowRenderPass.initialize(device, m_shadowMap.getFormat());

    ASH::FramebufferDesc shadowFbDesc{};
    shadowFbDesc.renderPass = m_shadowRenderPass.getRenderPass();
    shadowFbDesc.depthStencilAttachment = m_shadowMap.getTexture();
    shadowFbDesc.extent = { m_shadowMap.getSize(), m_shadowMap.getSize() };
    shadowFbDesc.layers = 1;
    m_shadowFramebuffer = device->createFramebuffer(shadowFbDesc);

    PipelineConfig shadowConfig{};
    shadowConfig.vertexShader = "shadow";
    shadowConfig.fragmentShader = "";
    shadowConfig.renderPass = m_shadowRenderPass.getRenderPass();
    shadowConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    shadowConfig.colorAttachmentCount = 0;
    shadowConfig.cullMode = ASH::CullMode::Front;
    shadowConfig.depthBiasEnable = true;
    shadowConfig.depthBiasConstant = 1.25f;
    shadowConfig.depthBiasSlope = 1.75f;
    m_shadowPipeline.initialize(device, shadowConfig);

    m_pointShadowRenderPass.initialize(device, ASH::Format::D32_Float);

    m_shadowAtlas.initialize(device);

    m_irradianceCubemap.initialize(device, 1, ASH::Format::R32G32B32A32_Float, 1);
    m_irradianceCubemap.clear(device, 0.0f, 0.0f, 0.0f, 1.0f);

    ASH::FramebufferDesc atlasFbDesc{};
    atlasFbDesc.renderPass = m_pointShadowRenderPass.getRenderPass();
    atlasFbDesc.depthStencilAttachment = m_shadowAtlas.getTexture();
    atlasFbDesc.extent = { m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() };
    atlasFbDesc.layers = 1;
    m_shadowAtlasFramebuffer = device->createFramebuffer(atlasFbDesc);

    PipelineConfig pointShadowConfig{};
    pointShadowConfig.vertexShader = "point_shadow";
    pointShadowConfig.fragmentShader = "point_shadow";
    pointShadowConfig.renderPass = m_pointShadowRenderPass.getRenderPass();
    pointShadowConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    pointShadowConfig.colorAttachmentCount = 0;
    pointShadowConfig.cullMode = ASH::CullMode::Front;
    pointShadowConfig.depthBiasEnable = true;
    pointShadowConfig.depthBiasConstant = 1.25f;
    pointShadowConfig.depthBiasSlope = 1.75f;
    pointShadowConfig.pushConstantSize = sizeof(glm::mat4) * 2 + sizeof(glm::vec4);
    pointShadowConfig.pushStage = ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment;
    m_pointShadowPipeline.initialize(device, pointShadowConfig);

    m_fogCameraBuffer.initialize(device, MAX_FRAMES_IN_FLIGHT);

    PipelineConfig fogConfig{};
    fogConfig.vertexShader = "fullscreen";
    fogConfig.fragmentShader = "fog";
    fogConfig.renderPass = m_hdrDecalRenderPass.getRenderPass();
    fogConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    fogConfig.colorAttachmentCount = 1;
    fogConfig.useVertexInput = false;
    fogConfig.layoutType = LayoutType::Fog;
    fogConfig.depthTest = false;
    fogConfig.depthWrite = false;
    fogConfig.cullMode = ASH::CullMode::None;
    fogConfig.blendEnable = true;
    fogConfig.blendSrcFactor = ASH::BlendFactor::SrcAlpha;
    fogConfig.blendDstFactor = ASH::BlendFactor::OneMinusSrcAlpha;
    fogConfig.blendOp = ASH::BlendOp::Add;
    fogConfig.pushConstantSize = sizeof(glm::vec4) * 4;
    fogConfig.pushStage = ASH::ShaderStage::Fragment;

    m_fogPipeline.initialize(device, fogConfig);
    createFogDescriptorSets();

    uint32_t imageCount = m_swapchain.getImageCount();
    m_imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphore.resize(imageCount);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(imageCount, nullptr);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_imageAvailableSemaphore[i] = device->createSemaphore();
        m_inFlightFences[i] = device->createFence(true);
    }
    for (uint32_t i = 0; i < imageCount; i++)
        m_renderFinishedSemaphore[i] = device->createSemaphore();

    {
        auto cmd = device->createCommandBuffer();
        cmd->begin();

        {
            ASH::ClearColor colorClear{};
            colorClear.f32[0] = 0.0f; colorClear.f32[1] = 0.0f; colorClear.f32[2] = 0.0f; colorClear.f32[3] = 1.0f;
            std::array<ASH::ClearColor, 2> hdrColorClears = { colorClear, colorClear };
            ASH::ClearDepthStencil depthClear{ 1.0f, 0 };

            ASH::RenderPassBeginInfo hdrPass{};
            hdrPass.renderPass = m_hdrRenderPass.getRenderPass();
            hdrPass.framebuffer = m_hdrFramebuffer.get();
            hdrPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
            hdrPass.colorClearValues = hdrColorClears.data();
            hdrPass.colorClearCount = 2;
            hdrPass.depthStencilClear = &depthClear;

            cmd->beginRenderPass(hdrPass);
            cmd->endRenderPass();
        }

        for (uint32_t slot = 0; slot < 2; slot++)
        {
            ASH::ClearColor colorClear{};
            colorClear.f32[0] = 0.0f; colorClear.f32[1] = 0.0f; colorClear.f32[2] = 0.0f; colorClear.f32[3] = 1.0f;

            ASH::RenderPassBeginInfo pass{};
            pass.renderPass = m_postRenderPass.getRenderPass();
            pass.framebuffer = m_taaFramebuffers[slot].get();
            pass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
            pass.colorClearValues = &colorClear;
            pass.colorClearCount = 1;

            cmd->beginRenderPass(pass);
            cmd->endRenderPass();

            ASH::TextureBarrier tb{};
            tb.texture = m_historyBuffer.getTexture(slot);
            tb.oldState = ASH::ResourceState::ColorAttachment;
            tb.newState = ASH::ResourceState::ShaderReadOnly;
            cmd->barrier(&tb, 1, nullptr, 0);
        }

        {
            ASH::ClearDepthStencil shadowClear{ 1.0f, 0 };

            ASH::RenderPassBeginInfo atlasPass{};
            atlasPass.renderPass = m_pointShadowRenderPass.getRenderPass();
            atlasPass.framebuffer = m_shadowAtlasFramebuffer.get();
            atlasPass.renderArea = { 0, 0, m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() };
            atlasPass.depthStencilClear = &shadowClear;

            cmd->beginRenderPass(atlasPass);
            cmd->endRenderPass();
        }

        {
            ASH::ClearColor clear{};
            clear.f32[0] = 0.0f; clear.f32[1] = 0.0f; clear.f32[2] = 0.0f; clear.f32[3] = 0.0f;

            ASH::RenderPassBeginInfo maskPass{};
            maskPass.renderPass = m_postRenderPass.getRenderPass();
            maskPass.framebuffer = m_godRayMaskFramebuffer.get();
            maskPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
            maskPass.colorClearValues = &clear;
            maskPass.colorClearCount = 1;

            cmd->beginRenderPass(maskPass);
            cmd->endRenderPass();

            ASH::RenderPassBeginInfo blurPass{};
            blurPass.renderPass = m_postRenderPass.getRenderPass();
            blurPass.framebuffer = m_godRayBlurFramebuffer.get();
            blurPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
            blurPass.colorClearValues = &clear;
            blurPass.colorClearCount = 1;

            cmd->beginRenderPass(blurPass);
            cmd->endRenderPass();
        }

        cmd->end();

        ASH::SubmitInfo submitInfo{};
        submitInfo.commandBuffer = cmd.get();
        device->submit(submitInfo);
        device->waitIdle();
    }
}

void Renderer::initializeVoxelGI(glm::vec3 gridMin, glm::vec3 gridSize, uint32_t resolution)
{
    ASH::Device* device = m_context.getDevice();
    m_voxelGI.initialize(device, resolution, gridMin, gridSize);
    m_hasVoxelGI = true;

    struct VoxelGridData
    {
        glm::vec4 gridMin;
        glm::vec4 gridSize;
    };

    VoxelGridData gridData{};
    gridData.gridMin = glm::vec4(gridMin, 0.0f);
    gridData.gridSize = glm::vec4(gridSize, 0.0f);

    ASH::BufferDesc bufDesc{};
    bufDesc.size = sizeof(VoxelGridData);
    bufDesc.usage = ASH::BufferUsage::UniformBuffer;
    bufDesc.memory = ASH::MemoryUsage::CpuToGpu;
    m_voxelGridBuffer = device->createBuffer(bufDesc);

    void* mapped = m_voxelGridBuffer->map();
    memcpy(mapped, &gridData, sizeof(VoxelGridData));
    m_voxelGridBuffer->unmap();
}

void Renderer::voxelizeScene(const Scene& scene)
{
    if (!m_hasVoxelGI) return;

    ASH::Device* device = m_context.getDevice();
    auto cmd = device->createCommandBuffer();
    cmd->begin();

    m_voxelGI.clearVoxels(cmd.get());
    m_voxelGI.voxelizeScene(cmd.get(), scene);

    ASH::TextureBarrier tb{};
    tb.texture = m_voxelGI.getVoxelTexture();
    tb.oldState = ASH::ResourceState::General;
    tb.newState = ASH::ResourceState::ShaderReadOnly;
    cmd->barrier(&tb, 1, nullptr, 0);

    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();
}

void Renderer::loadEnvironmentMap(const std::string& hdrPath)
{
    ASH::Device* device = m_context.getDevice();

    m_envBaker.initialize(device);
    m_environmentCubemap.initialize(device, 512, ASH::Format::R32G32B32A32_Float, 1);
    m_envBaker.bakeEnvironmentCubemap(device, hdrPath, m_environmentCubemap);

    m_irradianceCubemap.initialize(device, 32, ASH::Format::R32G32B32A32_Float, 1);
    m_envBaker.bakeIrradianceMap(device, m_environmentCubemap, m_irradianceCubemap);

    PipelineConfig skyboxConfig{};
    skyboxConfig.vertexShader = "skybox";
    skyboxConfig.fragmentShader = "skybox";
    skyboxConfig.renderPass = m_hdrRenderPass.getRenderPass();
    skyboxConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    skyboxConfig.layoutType = LayoutType::Skybox;
    skyboxConfig.depthCompareOp = ASH::CompareOp::LessOrEqual;
    skyboxConfig.colorAttachmentCount = 2;
    skyboxConfig.depthTest = true;
    skyboxConfig.depthWrite = false;
    skyboxConfig.cullMode = ASH::CullMode::None;
    skyboxConfig.useVertexInput = true;
    m_skyboxPipeline.initialize(device, skyboxConfig);

    m_skyboxDescriptorSet = m_skyboxPipeline.createDescriptorSet();

    ASH::DescriptorBufferInfo camInfo{ m_uniformBuffer.getBuffer(0), 0, m_uniformBuffer.getBuffer(0)->getSize() };
    ASH::DescriptorImageInfo envInfo{ m_environmentCubemap.getTexture(), m_environmentCubemap.getSampler() };

    std::array<ASH::DescriptorWrite, 2> writes{};
    writes[0] = { 0, ASH::DescriptorType::UniformBuffer, 0, &camInfo, nullptr };
    writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &envInfo };
    m_skyboxDescriptorSet->update(writes.data(), (uint32_t)writes.size());

    m_hasEnvironment = true;
}

void Renderer::createFogDescriptorSets()
{
    ASH::Device* device = m_context.getDevice();

    m_fogDescriptorSets.clear();
    m_fogDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_fogDescriptorSets[i] = m_fogPipeline.createDescriptorSet();

        ASH::DescriptorImageInfo depthInfo{ m_depthBuffer.getTexture(), m_depthBuffer.getSampler() };
        ASH::DescriptorImageInfo shadowInfo{ m_shadowMap.getTexture(), m_shadowMap.getSampler() };
        ASH::DescriptorBufferInfo camInfo{ m_fogCameraBuffer.getBuffer(i), 0, m_fogCameraBuffer.getBuffer(i)->getSize() };
        ASH::DescriptorBufferInfo lightInfo{ m_lightBuffer.getBuffer(i), 0, m_lightBuffer.getBuffer(i)->getSize() };

        std::array<ASH::DescriptorWrite, 4> writes{};
        writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &depthInfo };
        writes[1] = { 1, ASH::DescriptorType::UniformBuffer, 0, &camInfo, nullptr };
        writes[2] = { 2, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowInfo };
        writes[3] = { 3, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };

        m_fogDescriptorSets[i]->update(writes.data(), (uint32_t)writes.size());
    }
}

std::vector<ASH::Buffer*> Renderer::getUboBuffers() const
{
    std::vector<ASH::Buffer*> result;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        result.push_back(m_uniformBuffer.getBuffer(i));
    return result;
}

std::vector<ASH::Buffer*> Renderer::getLightBuffers() const
{
    std::vector<ASH::Buffer*> result;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        result.push_back(m_lightBuffer.getBuffer(i));
    return result;
}

float Renderer::getAspect() const
{
    return (float)m_swapchain.getExtent().width / (float)m_swapchain.getExtent().height;
}

void Renderer::drawScene(const Scene& scene, const Camera& camera)
{
    ASH::Device* device = m_context.getDevice();
    ASH::Fence* fence = m_inFlightFences[m_currentFrame].get();

    fence->wait();

    uint32_t imageIndex;
    if (!m_swapchain.acquireNextImage(imageIndex, m_imageAvailableSemaphore[m_currentFrame].get()))
    {
        ImGui::Render();
        recreateSwapchain();
        return;
    }

    if (m_imagesInFlight[imageIndex] != nullptr)
        m_imagesInFlight[imageIndex]->wait();

    m_imagesInFlight[imageIndex] = fence;
    fence->reset();

    ASH::CommandBuffer* cmd = m_commandManager.getCommandBuffer(m_currentFrame);

    glm::vec3 lightDir = glm::normalize(glm::vec3(m_pendingLights.lights[0].position));
    float orthSize = 20.0f;
    glm::mat4 lightProj = glm::ortho(-orthSize, orthSize, -orthSize, orthSize, 1.0f, 100.0f);
    lightProj[1][1] *= -1;
    glm::vec3 lightPos = -lightDir * 30.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    m_lightSpaceMatrix = lightProj * lightView;

    struct LightDist { int index; float dist2; };
    std::vector<LightDist> pointLights;

    for (int i = 0; i < m_pendingLights.count.x; i++)
    {
        if (m_pendingLights.lights[i].position.w >= 0.5f)
        {
            glm::vec3 lp = glm::vec3(m_pendingLights.lights[i].position);
            float dist2 = glm::dot(lp - camera.getPosition(), lp - camera.getPosition());
            pointLights.push_back({ i, dist2 });
        }
    }

    std::sort(pointLights.begin(), pointLights.end(), [](const LightDist& a, const LightDist& b) {
        return a.dist2 < b.dist2;
    });

    m_numActivePointShadows = std::min((int)pointLights.size(), NUM_POINT_SHADOWS);

    for (int slot = 0; slot < m_numActivePointShadows; slot++)
    {
        int lightIdx = pointLights[slot].index;
        glm::vec3 lp = glm::vec3(m_pendingLights.lights[lightIdx].position);
        m_activePointLightPos[slot] = lp;
        m_activePointLightIndex[slot] = lightIdx;
        updatePointLightMatrices(slot, lp);
    }

    m_hasPointLightShadow = m_numActivePointShadows > 0;

    m_pendingLights.count.y = (m_numActivePointShadows > 0) ? m_activePointLightIndex[0] : -1;
    m_pendingLights.count.z = (m_numActivePointShadows > 1) ? m_activePointLightIndex[1] : -1;
    m_pendingLights.count.w = (m_numActivePointShadows > 2) ? m_activePointLightIndex[2] : -1;

    glm::vec2 jitter = (m_aaMethod == AAMethod::TAA) ? getJitter() : glm::vec2(0.0f);
    glm::mat4 baseProj = camera.getProjectionMatrix();

    glm::mat4 proj = baseProj;
    ASH::Extent2D extent = m_swapchain.getExtent();
    proj[2][0] += jitter.x / (float)extent.width * 2.0f;
    proj[2][1] += jitter.y / (float)extent.height * 2.0f;

    MVP mvp{};
    mvp.view = camera.getViewMatrix();
    mvp.proj = proj;
    mvp.lightSpaceMatrix = m_lightSpaceMatrix;
    mvp.prevViewProj = m_prevViewProj;
    mvp.viewProjNoJitter = baseProj * camera.getViewMatrix();
    mvp.camPos = glm::vec4(camera.getPosition(), 1.0f);
    mvp.iblParams = glm::vec4(m_iblSettings.diffuseIntensity, m_iblSettings.specularIntensity, 0.0f, 0.0f);
    m_uniformBuffer.update(m_currentFrame, mvp);

    m_prevViewProj = mvp.viewProjNoJitter;
    m_lightBuffer.update(m_currentFrame, m_pendingLights);

    DecalCameraData decalCamData{};
    decalCamData.view = camera.getViewMatrix();
    decalCamData.proj = baseProj;
    decalCamData.invViewProj = glm::inverse(decalCamData.proj * decalCamData.view);
    decalCamData.camPos = glm::vec4(camera.getPosition(), 1.0f);
    m_decalCameraBuffer.update(m_currentFrame, decalCamData);

    recordCommandBuffer(cmd, imageIndex, scene, camera);

    ASH::SubmitInfo submitInfo{};
    submitInfo.waitSemaphore = m_imageAvailableSemaphore[m_currentFrame].get();
    submitInfo.commandBuffer = cmd;
    submitInfo.signalSemaphore = m_renderFinishedSemaphore[imageIndex].get();
    submitInfo.signalFence = fence;
    device->submit(submitInfo);

    m_swapchain.present(imageIndex, m_renderFinishedSemaphore[imageIndex].get());

    if (m_framebufferResized)
    {
        m_framebufferResized = false;
        recreateSwapchain();
    }

    m_frameIndex++;
    m_historyIndex = 1 - m_historyIndex;
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recordCommandBuffer(ASH::CommandBuffer* cmd, uint32_t imageIndex,
    const Scene& scene, const Camera& camera)
{
    cmd->begin();

    auto transition = [&](ASH::Texture* tex, ASH::ResourceState from, ASH::ResourceState to)
    {
        ASH::TextureBarrier tb{};
        tb.texture = tex;
        tb.oldState = from;
        tb.newState = to;
        cmd->barrier(&tb, 1, nullptr, 0);
    };

    {
        ASH::ClearDepthStencil shadowClear{ 1.0f, 0 };

        ASH::RenderPassBeginInfo shadowPass{};
        shadowPass.renderPass = m_shadowRenderPass.getRenderPass();
        shadowPass.framebuffer = m_shadowFramebuffer.get();
        shadowPass.renderArea = { 0, 0, m_shadowMap.getSize(), m_shadowMap.getSize() };
        shadowPass.depthStencilClear = &shadowClear;

        cmd->beginRenderPass(shadowPass);
        cmd->bindPipeline(m_shadowPipeline.getPipeline());

        ASH::Viewport vp{};
        vp.width = (float)m_shadowMap.getSize();
        vp.height = (float)m_shadowMap.getSize();
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        cmd->setViewport(vp);

        ASH::Rect2D sc{ 0, 0, m_shadowMap.getSize(), m_shadowMap.getSize() };
        cmd->setScissor(sc);

        for (const auto& obj : scene.getObjects())
        {
            if (!obj->mesh) continue;
            obj->mesh->bind(cmd);

            struct ShadowPush { glm::mat4 lightSpace; glm::mat4 model; } spush;
            spush.lightSpace = m_lightSpaceMatrix;
            spush.model = obj->transform.getMatrix();
            cmd->pushConstants(m_shadowPipeline.getPipeline(), ASH::ShaderStage::Vertex, 0, sizeof(ShadowPush), &spush);

            for (const auto& sm : obj->mesh->getSubmeshes())
                obj->mesh->drawSubmesh(cmd, sm);
        }
        cmd->endRenderPass();
        transition(m_shadowMap.getTexture(), ASH::ResourceState::DepthStencilAttachment, ASH::ResourceState::ShaderReadOnly);
    }

    if (m_hasPointLightShadow)
    {
        ASH::ClearDepthStencil shadowClear{ 1.0f, 0 };

        ASH::RenderPassBeginInfo atlasPass{};
        atlasPass.renderPass = m_pointShadowRenderPass.getRenderPass();
        atlasPass.framebuffer = m_shadowAtlasFramebuffer.get();
        atlasPass.renderArea = { 0, 0, m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() };
        atlasPass.depthStencilClear = &shadowClear;

        cmd->beginRenderPass(atlasPass);
        cmd->bindPipeline(m_pointShadowPipeline.getPipeline());

        for (int slot = 0; slot < m_numActivePointShadows; slot++)
        {
            for (uint32_t face = 0; face < 6; face++)
            {
                ASH::Rect2D tile = m_shadowAtlas.getTileRect(slot, face);

                ASH::Viewport vp{};
                vp.x = (float)tile.x; vp.y = (float)tile.y;
                vp.width = (float)tile.width; vp.height = (float)tile.height;
                vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
                cmd->setViewport(vp);
                cmd->setScissor(tile);

                auto facePlanes = extractFrustumPlanes(m_pointLightViewProjArr[slot][face]);

                for (const auto& obj : scene.getObjects())
                {
                    if (!obj->mesh) continue;
                    obj->mesh->bind(cmd);

                    glm::mat4 modelMatrix = obj->transform.getMatrix();

                    PointShadowPush pspush{};
                    pspush.lightViewProj = m_pointLightViewProjArr[slot][face];
                    pspush.model = modelMatrix;
                    pspush.lightPos = glm::vec4(m_activePointLightPos[slot], m_pointShadowFarPlane);

                    cmd->pushConstants(m_pointShadowPipeline.getPipeline(),
                        ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment,
                        0, sizeof(PointShadowPush), &pspush);

                    for (const auto& sm : obj->mesh->getSubmeshes())
                    {
                        if (!isSubmeshVisible(modelMatrix, sm, facePlanes))
                            continue;
                        obj->mesh->drawSubmesh(cmd, sm);
                    }
                }
            }
        }
        cmd->endRenderPass();
        transition(m_shadowAtlas.getTexture(), ASH::ResourceState::DepthStencilAttachment, ASH::ResourceState::ShaderReadOnly);
    }
    else
    {
        ASH::ClearDepthStencil shadowClear{ 1.0f, 0 };

        ASH::RenderPassBeginInfo atlasPass{};
        atlasPass.renderPass = m_pointShadowRenderPass.getRenderPass();
        atlasPass.framebuffer = m_shadowAtlasFramebuffer.get();
        atlasPass.renderArea = { 0, 0, m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() };
        atlasPass.depthStencilClear = &shadowClear;

        cmd->beginRenderPass(atlasPass);
        cmd->endRenderPass();
    }
    {
        ASH::ClearColor colorClear0{};
        colorClear0.f32[0] = 0.0f; colorClear0.f32[1] = 0.0f; colorClear0.f32[2] = 0.0f; colorClear0.f32[3] = 1.0f;
        std::array<ASH::ClearColor, 2> clearValues = { colorClear0, colorClear0 };
        ASH::ClearDepthStencil depthClear{ 1.0f, 0 };

        ASH::RenderPassBeginInfo hdrPass{};
        hdrPass.renderPass = m_hdrRenderPass.getRenderPass();
        hdrPass.framebuffer = m_hdrFramebuffer.get();
        hdrPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        hdrPass.colorClearValues = clearValues.data();
        hdrPass.colorClearCount = 2;
        hdrPass.depthStencilClear = &depthClear;

        cmd->beginRenderPass(hdrPass);

        ASH::Viewport viewport{};
        viewport.width = (float)m_swapchain.getExtent().width;
        viewport.height = (float)m_swapchain.getExtent().height;
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        cmd->setViewport(viewport);

        ASH::Rect2D scissor{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(scissor);

        auto frustumPlanes = extractFrustumPlanes(camera.getProjectionMatrix() * camera.getViewMatrix());

        int visibleCount = 0;
        int totalCount = 0;

        ASH::Pipeline* currentBoundPipeline = nullptr;

        if (m_hasEnvironment)
        {
            cmd->bindPipeline(m_skyboxPipeline.getPipeline());
            cmd->bindDescriptorSet(m_skyboxPipeline.getPipeline(), 0, m_skyboxDescriptorSet.get());
            m_decalBox.bind(cmd);
            m_decalBox.draw(cmd);
        }

        for (const auto& obj : scene.getObjects())
        {
            auto* skeletal = obj->getComponent<SkeletalMeshComponent>();

            if (!obj->mesh && !skeletal) continue;

            glm::mat4 modelMatrix = obj->transform.getMatrix();

            PushConstant pc{};
            pc.model = modelMatrix;
            pc.normalMatrix = obj->transform.getNormalMatrix();
            pc.prevModel = modelMatrix;

            ASH::Pipeline* activePipeline = skeletal ? m_skinnedPipeline.getPipeline() : m_pipeline.getPipeline();

            if (skeletal)
            {
                auto skeletalMesh = skeletal->getSkeletalMesh();
                if (!skeletalMesh) continue;

                skeletal->uploadBoneMatrices(m_currentFrame);

                if (currentBoundPipeline != activePipeline)
                {
                    cmd->bindPipeline(activePipeline);
                    currentBoundPipeline = activePipeline;
                }

                skeletalMesh->bind(cmd);

                cmd->pushConstants(activePipeline, ASH::ShaderStage::Vertex, 0, sizeof(PushConstant), &pc);

                for (const auto& sm : skeletalMesh->getSubmeshes())
                {
                    totalCount++;
                    visibleCount++;

                    auto mat = obj->getMaterial(sm.materialIndex);
                    if (!mat) continue;

                    cmd->bindDescriptorSet(activePipeline, 0, mat->getDescriptorSet(m_currentFrame));

                    skeletalMesh->drawSubmesh(cmd, sm);
                }
            }
            else
            {
                if (!obj->mesh) continue;

                if (currentBoundPipeline != activePipeline)
                {
                    cmd->bindPipeline(activePipeline);
                    currentBoundPipeline = activePipeline;
                }

                obj->mesh->bind(cmd);

                cmd->pushConstants(activePipeline, ASH::ShaderStage::Vertex, 0, sizeof(PushConstant), &pc);

                for (const auto& sm : obj->mesh->getSubmeshes())
                {
                    totalCount++;

                    if (!isSubmeshVisible(modelMatrix, sm, frustumPlanes))
                        continue;

                    visibleCount++;

                    auto mat = obj->getMaterial(sm.materialIndex);
                    if (!mat) continue;

                    cmd->bindDescriptorSet(activePipeline, 0, mat->getDescriptorSet(m_currentFrame));

                    obj->mesh->drawSubmesh(cmd, sm);
                }
            }
        }


        cmd->endRenderPass();
    }

    if (!m_screenSpaceDecals.empty())
    {
        ASH::RenderPassBeginInfo decalPass{};
        decalPass.renderPass = m_hdrDecalRenderPass.getRenderPass();
        decalPass.framebuffer = m_hdrDecalFramebuffer.get();
        decalPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };

        cmd->beginRenderPass(decalPass);

        ASH::Viewport viewport{};
        viewport.width = (float)m_swapchain.getExtent().width;
        viewport.height = (float)m_swapchain.getExtent().height;
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        cmd->setViewport(viewport);

        ASH::Rect2D scissor{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(scissor);

        cmd->bindPipeline(m_decalPipeline.getPipeline());
        m_decalBox.bind(cmd);

        for (const auto& decal : m_screenSpaceDecals)
        {
            struct DecalPush { glm::mat4 model; glm::mat4 invModel; glm::vec4 params; } dpush;
            dpush.model = decal->getModelMatrix();
            dpush.invModel = decal->getInvModelMatrix();
            dpush.params = glm::vec4(decal->metallic, decal->roughness, 0.0f, 0.0f);
            cmd->pushConstants(m_decalPipeline.getPipeline(),
                ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment,
                0, sizeof(DecalPush), &dpush);

            cmd->bindDescriptorSet(m_decalPipeline.getPipeline(), 0, decal->getDescriptorSet(m_currentFrame));

            m_decalBox.draw(cmd);
        }

        cmd->endRenderPass();
    }

    if (m_fogSettings.enabled)
    {
        FogCameraData fogCam{};
        fogCam.view = camera.getViewMatrix();
        fogCam.proj = camera.getProjectionMatrix();
        fogCam.invViewProj = glm::inverse(fogCam.proj * fogCam.view);
        fogCam.lightSpaceMatrix = m_lightSpaceMatrix;
        fogCam.camPos = glm::vec4(camera.getPosition(), 1.0f);
        m_fogCameraBuffer.update(m_currentFrame, fogCam);

        ASH::RenderPassBeginInfo fogPass{};
        fogPass.renderPass = m_hdrDecalRenderPass.getRenderPass();
        fogPass.framebuffer = m_hdrDecalFramebuffer.get();
        fogPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };

        cmd->beginRenderPass(fogPass);

        ASH::Viewport viewport{};
        viewport.width = (float)m_swapchain.getExtent().width;
        viewport.height = (float)m_swapchain.getExtent().height;
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        cmd->setViewport(viewport);

        ASH::Rect2D scissor{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(scissor);

        cmd->bindPipeline(m_fogPipeline.getPipeline());
        cmd->bindDescriptorSet(m_fogPipeline.getPipeline(), 0, m_fogDescriptorSets[m_currentFrame].get());

        struct FogPush
        {
            glm::vec4 scatteringColorDensity;
            glm::vec4 heightParams;
            glm::vec4 lightParams;
            glm::vec4 miscParams;
        } fogPush{};

        fogPush.scatteringColorDensity = glm::vec4(m_fogSettings.scatteringColor, m_fogSettings.density);
        fogPush.heightParams = glm::vec4(m_fogSettings.heightFalloff, m_fogSettings.heightStart,
            m_fogSettings.maxDistance, (float)m_fogSettings.stepCount);
        fogPush.lightParams = glm::vec4(m_fogSettings.ambientIntensity, m_fogSettings.sunIntensity,
            m_fogSettings.noiseScale, m_fogSettings.noiseSpeed);
        fogPush.miscParams = glm::vec4(m_totalTime, 0.0f, 0.0f, 0.0f);

        cmd->pushConstants(m_fogPipeline.getPipeline(), ASH::ShaderStage::Fragment,
            0, sizeof(FogPush), &fogPush);

        cmd->draw(3, 1, 0, 0);
        cmd->endRenderPass();
    }

    if (m_godRaySettings.enabled)
    {
        bool sunVisible = false;
        glm::vec3 sunDir(0.0f, 1.0f, 0.0f);
        glm::vec3 sunColor(1.0f);
        glm::vec2 sunUV(0.5f);

        if (m_pendingLights.count.x > 0 && m_pendingLights.lights[0].position.w < 0.5f)
        {
            sunDir = glm::normalize(-glm::vec3(m_pendingLights.lights[0].position));
            sunColor = glm::vec3(m_pendingLights.lights[0].color);

            glm::mat4 proj = camera.getProjectionMatrix();
            glm::mat4 view = camera.getViewMatrix();
            glm::vec3 sunFarPoint = camera.getPosition() + sunDir * 1000.0f;
            glm::vec4 clip = proj * view * glm::vec4(sunFarPoint, 1.0f);

            if (clip.w > 0.0f)
            {
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                sunUV = glm::vec2(ndc.x * 0.5f + 0.5f, ndc.y * 0.5f + 0.5f);
                sunVisible = true;
            }
        }

        if (sunVisible)
        {
            ASH::Extent2D grExtent = m_swapchain.getExtent();

            {
                ASH::ClearColor clear{};
                clear.f32[0] = 0; clear.f32[1] = 0; clear.f32[2] = 0; clear.f32[3] = 0;

                ASH::RenderPassBeginInfo pass{};
                pass.renderPass = m_postRenderPass.getRenderPass();
                pass.framebuffer = m_godRayMaskFramebuffer.get();
                pass.renderArea = { 0, 0, grExtent.width, grExtent.height };
                pass.colorClearValues = &clear;
                pass.colorClearCount = 1;

                cmd->beginRenderPass(pass);
                cmd->bindPipeline(m_godRayMaskPipeline.getPipeline());

                ASH::Viewport vp{};
                vp.width = (float)grExtent.width; vp.height = (float)grExtent.height; vp.maxDepth = 1.0f;
                cmd->setViewport(vp);
                ASH::Rect2D sc{ 0, 0, grExtent.width, grExtent.height };
                cmd->setScissor(sc);

                cmd->bindDescriptorSet(m_godRayMaskPipeline.getPipeline(), 0, m_godRayMaskDescriptorSets[m_currentFrame].get());

                struct MaskPush { glm::mat4 invViewProj; glm::vec4 sunDirIntensity; glm::vec4 sunColorSize; } maskPush{};
                maskPush.invViewProj = glm::inverse(camera.getProjectionMatrix() * camera.getViewMatrix());
                maskPush.sunDirIntensity = glm::vec4(sunDir, 1.0f);
                maskPush.sunColorSize = glm::vec4(sunColor, m_godRaySettings.sunSize);

                cmd->pushConstants(m_godRayMaskPipeline.getPipeline(), ASH::ShaderStage::Fragment,
                    0, sizeof(MaskPush), &maskPush);

                cmd->draw(3, 1, 0, 0);
                cmd->endRenderPass();
                transition(m_godRayMaskTarget.getColorTexture(), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
            }

            {
                ASH::ClearColor clear{};
                clear.f32[0] = 0; clear.f32[1] = 0; clear.f32[2] = 0; clear.f32[3] = 0;

                ASH::RenderPassBeginInfo pass{};
                pass.renderPass = m_postRenderPass.getRenderPass();
                pass.framebuffer = m_godRayBlurFramebuffer.get();
                pass.renderArea = { 0, 0, grExtent.width, grExtent.height };
                pass.colorClearValues = &clear;
                pass.colorClearCount = 1;

                cmd->beginRenderPass(pass);
                cmd->bindPipeline(m_godRayBlurPipeline.getPipeline());

                ASH::Viewport vp{};
                vp.width = (float)grExtent.width; vp.height = (float)grExtent.height; vp.maxDepth = 1.0f;
                cmd->setViewport(vp);
                ASH::Rect2D sc{ 0, 0, grExtent.width, grExtent.height };
                cmd->setScissor(sc);

                cmd->bindDescriptorSet(m_godRayBlurPipeline.getPipeline(), 0, m_godRayBlurDescriptorSets[m_currentFrame].get());

                struct BlurPush { glm::vec4 sunUVParams; glm::vec4 weightCountParams; } blurPush{};
                blurPush.sunUVParams = glm::vec4(sunUV, m_godRaySettings.density, m_godRaySettings.decay);
                blurPush.weightCountParams = glm::vec4(m_godRaySettings.weight, (float)m_godRaySettings.sampleCount, 0.0f, 0.0f);

                cmd->pushConstants(m_godRayBlurPipeline.getPipeline(), ASH::ShaderStage::Fragment,
                    0, sizeof(BlurPush), &blurPush);

                cmd->draw(3, 1, 0, 0);
                cmd->endRenderPass();
                transition(m_godRayBlurTarget.getColorTexture(), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
            }

            {
                ASH::RenderPassBeginInfo pass{};
                pass.renderPass = m_hdrDecalRenderPass.getRenderPass();
                pass.framebuffer = m_hdrDecalFramebuffer.get();
                pass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };

                cmd->beginRenderPass(pass);
                cmd->bindPipeline(m_godRayCompositePipeline.getPipeline());

                ASH::Viewport vp{};
                vp.width = (float)m_swapchain.getExtent().width; vp.height = (float)m_swapchain.getExtent().height; vp.maxDepth = 1.0f;
                cmd->setViewport(vp);
                ASH::Rect2D sc{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
                cmd->setScissor(sc);

                cmd->bindDescriptorSet(m_godRayCompositePipeline.getPipeline(), 0, m_godRayCompositeDescriptorSets[m_currentFrame].get());

                float intensity = m_godRaySettings.intensity;
                cmd->pushConstants(m_godRayCompositePipeline.getPipeline(), ASH::ShaderStage::Fragment,
                    0, sizeof(float), &intensity);

                cmd->draw(3, 1, 0, 0);
                cmd->endRenderPass();
            }
        }
    }
    transition(m_hdrTarget.getColorTexture(), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
    transition(m_velocityTarget.getColorTexture(), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);

    if (m_aaMethod == AAMethod::TAA)
    {
        uint32_t writeSlot = m_historyIndex;

        ASH::ClearColor clear{};
        clear.f32[0] = 0.0f; clear.f32[1] = 0.0f; clear.f32[2] = 0.0f; clear.f32[3] = 1.0f;

        ASH::RenderPassBeginInfo pass{};
        pass.renderPass = m_postRenderPass.getRenderPass();
        pass.framebuffer = m_taaFramebuffers[writeSlot].get();
        pass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        pass.colorClearValues = &clear;
        pass.colorClearCount = 1;

        cmd->beginRenderPass(pass);
        cmd->bindPipeline(m_taaPipeline.getPipeline());

        ASH::Viewport vp{};
        vp.width = (float)m_swapchain.getExtent().width; vp.height = (float)m_swapchain.getExtent().height;
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        cmd->setViewport(vp);
        ASH::Rect2D sc{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(sc);

        cmd->bindDescriptorSet(m_taaPipeline.getPipeline(), 0, m_taaDescriptorSets[writeSlot][m_currentFrame].get());

        cmd->pushConstants(m_taaPipeline.getPipeline(), ASH::ShaderStage::Fragment, 0, sizeof(float), &m_taaBlendFactor);

        cmd->draw(3, 1, 0, 0);
        cmd->endRenderPass();
        transition(m_historyBuffer.getTexture(writeSlot), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
    }

    {
        ASH::ClearColor clear{};
        clear.f32[0] = 0.0f; clear.f32[1] = 0.0f; clear.f32[2] = 0.0f; clear.f32[3] = 1.0f;

        ASH::RenderPassBeginInfo pass{};
        pass.renderPass = m_postRenderPass.getRenderPass();
        pass.framebuffer = m_brightFramebuffer.get();
        pass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        pass.colorClearValues = &clear;
        pass.colorClearCount = 1;

        cmd->beginRenderPass(pass);
        cmd->bindPipeline(m_brightPipeline.getPipeline());

        ASH::Viewport vp{};
        vp.width = (float)m_swapchain.getExtent().width; vp.height = (float)m_swapchain.getExtent().height;
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        cmd->setViewport(vp);
        ASH::Rect2D sc{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(sc);

        ASH::DescriptorSet* brightSet = (m_aaMethod == AAMethod::TAA)
            ? m_brightDescriptorSets[m_historyIndex][m_currentFrame].get()
            : m_brightDescriptorSetsRaw[m_currentFrame].get();

        cmd->bindDescriptorSet(m_brightPipeline.getPipeline(), 0, brightSet);

        cmd->pushConstants(m_brightPipeline.getPipeline(), ASH::ShaderStage::Fragment, 0, sizeof(float), &m_bloomThreshold);

        cmd->draw(3, 1, 0, 0);
        cmd->endRenderPass();
        transition(m_brightTarget.getColorTexture(), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
    }

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        ASH::ClearColor clear{};
        clear.f32[0] = 0.0f; clear.f32[1] = 0.0f; clear.f32[2] = 0.0f; clear.f32[3] = 1.0f;

        ASH::RenderPassBeginInfo pass{};
        pass.renderPass = m_postRenderPass.getRenderPass();
        pass.framebuffer = m_bloomFramebuffers[level].get();
        ASH::Extent2D mipExtent = m_bloomChain.getMipExtent(level);
        pass.renderArea = { 0, 0, mipExtent.width, mipExtent.height };
        pass.colorClearValues = &clear;
        pass.colorClearCount = 1;

        cmd->beginRenderPass(pass);
        cmd->bindPipeline(m_downsamplePipeline.getPipeline());

        ASH::Viewport vp{};
        vp.width = (float)mipExtent.width; vp.height = (float)mipExtent.height;
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        cmd->setViewport(vp);
        ASH::Rect2D sc{ 0, 0, mipExtent.width, mipExtent.height };
        cmd->setScissor(sc);

        cmd->bindDescriptorSet(m_downsamplePipeline.getPipeline(), 0, m_downsampleDescriptorSets[level][m_currentFrame].get());

        ASH::Extent2D srcExtent = (level == 0) ? m_swapchain.getExtent() : m_bloomChain.getMipExtent(level - 1);
        glm::vec2 srcTexelSize(1.0f / srcExtent.width, 1.0f / srcExtent.height);
        cmd->pushConstants(m_downsamplePipeline.getPipeline(), ASH::ShaderStage::Fragment, 0, sizeof(glm::vec2), &srcTexelSize);

        cmd->draw(3, 1, 0, 0);
        cmd->endRenderPass();
        transition(m_bloomChain.getMipTexture(level), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
    }

    for (int level = (int)BloomChain::MIP_COUNT - 2; level >= 0; level--)
    {
        ASH::RenderPassBeginInfo pass{};
        pass.renderPass = m_postRenderPassLoad.getRenderPass();
        pass.framebuffer = m_bloomFramebuffers[level].get();
        ASH::Extent2D mipExtent = m_bloomChain.getMipExtent(level);
        pass.renderArea = { 0, 0, mipExtent.width, mipExtent.height };

        transition(m_bloomChain.getMipTexture(level), ASH::ResourceState::ShaderReadOnly, ASH::ResourceState::ColorAttachment);
        cmd->beginRenderPass(pass);
        cmd->bindPipeline(m_upsamplePipeline.getPipeline());

        ASH::Viewport vp{};
        vp.width = (float)mipExtent.width; vp.height = (float)mipExtent.height;
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        cmd->setViewport(vp);
        ASH::Rect2D sc{ 0, 0, mipExtent.width, mipExtent.height };
        cmd->setScissor(sc);

        cmd->bindDescriptorSet(m_upsamplePipeline.getPipeline(), 0, m_upsampleDescriptorSets[level][m_currentFrame].get());

        float radius = 1.0f;
        cmd->pushConstants(m_upsamplePipeline.getPipeline(), ASH::ShaderStage::Fragment, 0, sizeof(float), &radius);

        cmd->draw(3, 1, 0, 0);
        cmd->endRenderPass();
        transition(m_bloomChain.getMipTexture(level), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
    }

    {
        ASH::ClearColor clear{};
        clear.f32[0] = 0.0f; clear.f32[1] = 0.0f; clear.f32[2] = 0.0f; clear.f32[3] = 1.0f;

        ASH::RenderPassBeginInfo pass{};
        pass.renderPass = m_ldrRenderPass.getRenderPass();
        pass.framebuffer = m_ldrFramebuffer.get();
        pass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        pass.colorClearValues = &clear;
        pass.colorClearCount = 1;

        cmd->beginRenderPass(pass);
        cmd->bindPipeline(m_postPipeline.getPipeline());

        ASH::Viewport viewport{};
        viewport.width = (float)m_swapchain.getExtent().width;
        viewport.height = (float)m_swapchain.getExtent().height;
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        cmd->setViewport(viewport);

        ASH::Rect2D scissor{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(scissor);

        ASH::DescriptorSet* postSet = (m_aaMethod == AAMethod::TAA)
            ? m_postDescriptorSets[m_historyIndex][m_currentFrame].get()
            : m_postDescriptorSetsRaw[m_currentFrame].get();

        cmd->bindDescriptorSet(m_postPipeline.getPipeline(), 0, postSet);

        struct TonemapPush { float exposure; float contrast; } tmPush{ m_exposure, m_contrast };
        cmd->pushConstants(m_postPipeline.getPipeline(), ASH::ShaderStage::Fragment, 0, sizeof(TonemapPush), &tmPush);

        cmd->draw(3, 1, 0, 0);
        cmd->endRenderPass();
        transition(m_ldrTarget.getColorTexture(), ASH::ResourceState::ColorAttachment, ASH::ResourceState::ShaderReadOnly);
    }

    {
        ASH::ClearColor colorClear{};
        colorClear.f32[0] = 0.0f; colorClear.f32[1] = 0.0f; colorClear.f32[2] = 0.0f; colorClear.f32[3] = 1.0f;
        ASH::ClearDepthStencil depthClear{ 1.0f, 0 };

        ASH::RenderPassBeginInfo finalPass{};
        finalPass.renderPass = m_renderPass.getRenderPass();
        finalPass.framebuffer = m_framebuffers.getFramebuffers()[imageIndex].get();
        finalPass.renderArea = { 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        finalPass.colorClearValues = &colorClear;
        finalPass.colorClearCount = 1;
        finalPass.depthStencilClear = &depthClear;

        cmd->beginRenderPass(finalPass);

        ASH::Viewport viewport{};
        viewport.width = (float)m_swapchain.getExtent().width;
        viewport.height = (float)m_swapchain.getExtent().height;
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        cmd->setViewport(viewport);

        ASH::Rect2D scissor{ 0, 0, m_swapchain.getExtent().width, m_swapchain.getExtent().height };
        cmd->setScissor(scissor);

        m_fxaa.apply(cmd, m_currentFrame, m_ldrTarget.getColorTexture(), m_ldrTarget.getSampler());

        renderImGui(cmd);

        cmd->endRenderPass();
    }

    transition(m_swapchain.getImage(imageIndex), ASH::ResourceState::ColorAttachment, ASH::ResourceState::Present);
    cmd->end();
}

void Renderer::recreateSwapchain()
{
    LOG("recreateSwapchain() called!");
    ASH::Device* device = m_context.getDevice();
    device->waitIdle();

    m_framebuffers.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.recreate(m_context, m_window);
    ASH::Extent2D extent = m_swapchain.getExtent();

    m_godRayMaskFramebuffer.reset();
    m_godRayBlurFramebuffer.reset();
    m_godRayMaskTarget.cleanup();
    m_godRayBlurTarget.cleanup();
    m_godRayMaskTarget.initialize(device, extent);
    m_godRayBlurTarget.initialize(device, extent);

    ASH::FramebufferDesc maskFbDesc{};
    maskFbDesc.renderPass = m_postRenderPass.getRenderPass();
    maskFbDesc.colorAttachments = { m_godRayMaskTarget.getColorTexture() };
    maskFbDesc.extent = extent;
    maskFbDesc.layers = 1;
    m_godRayMaskFramebuffer = device->createFramebuffer(maskFbDesc);

    ASH::FramebufferDesc blurFbDesc{};
    blurFbDesc.renderPass = m_postRenderPass.getRenderPass();
    blurFbDesc.colorAttachments = { m_godRayBlurTarget.getColorTexture() };
    blurFbDesc.extent = extent;
    blurFbDesc.layers = 1;
    m_godRayBlurFramebuffer = device->createFramebuffer(blurFbDesc);

    m_depthBuffer.initialize(device, extent);
    m_framebuffers.initialize(device, m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getTexture());

    createFogDescriptorSets();

    m_velocityTarget.cleanup();
    m_velocityTarget.initialize(device, extent);

    m_hdrFramebuffer.reset();
    m_hdrTarget.cleanup();
    m_hdrTarget.initialize(device, extent);

    ASH::FramebufferDesc hdrFbDesc{};
    hdrFbDesc.renderPass = m_hdrRenderPass.getRenderPass();
    hdrFbDesc.colorAttachments = { m_hdrTarget.getColorTexture(), m_velocityTarget.getColorTexture() };
    hdrFbDesc.depthStencilAttachment = m_depthBuffer.getTexture();
    hdrFbDesc.extent = extent;
    hdrFbDesc.layers = 1;
    m_hdrFramebuffer = device->createFramebuffer(hdrFbDesc);

    m_hdrDecalFramebuffer.reset();
    ASH::FramebufferDesc hdrDecalFbDesc{};
    hdrDecalFbDesc.renderPass = m_hdrDecalRenderPass.getRenderPass();
    hdrDecalFbDesc.colorAttachments = { m_hdrTarget.getColorTexture() };
    hdrDecalFbDesc.extent = extent;
    hdrDecalFbDesc.layers = 1;
    m_hdrDecalFramebuffer = device->createFramebuffer(hdrDecalFbDesc);

    m_brightFramebuffer.reset();
    m_brightTarget.cleanup();
    m_brightTarget.initialize(device, extent);

    ASH::FramebufferDesc brightFbDesc{};
    brightFbDesc.renderPass = m_postRenderPass.getRenderPass();
    brightFbDesc.colorAttachments = { m_brightTarget.getColorTexture() };
    brightFbDesc.extent = extent;
    brightFbDesc.layers = 1;
    m_brightFramebuffer = device->createFramebuffer(brightFbDesc);

    for (auto& fb : m_bloomFramebuffers) fb.reset();
    m_bloomChain.cleanup();
    m_bloomChain.initialize(device, extent);

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        ASH::FramebufferDesc fbDesc{};
        fbDesc.renderPass = m_postRenderPass.getRenderPass();
        fbDesc.colorAttachments = { m_bloomChain.getMipTexture(level) };
        fbDesc.extent = m_bloomChain.getMipExtent(level);
        fbDesc.layers = 1;
        m_bloomFramebuffers[level] = device->createFramebuffer(fbDesc);
    }

    m_ldrFramebuffer.reset();
    m_ldrTarget.cleanup();
    m_ldrTarget.initialize(device, extent);

    ASH::FramebufferDesc ldrFbDesc{};
    ldrFbDesc.renderPass = m_ldrRenderPass.getRenderPass();
    ldrFbDesc.colorAttachments = { m_ldrTarget.getColorTexture() };
    ldrFbDesc.extent = extent;
    ldrFbDesc.layers = 1;
    m_ldrFramebuffer = device->createFramebuffer(ldrFbDesc);

    for (auto& fb : m_taaFramebuffers) fb.reset();
    m_historyBuffer.cleanup();
    m_historyBuffer.initialize(device, extent);

    for (uint32_t i = 0; i < 2; i++)
    {
        ASH::FramebufferDesc fbDesc{};
        fbDesc.renderPass = m_postRenderPass.getRenderPass();
        fbDesc.colorAttachments = { m_historyBuffer.getTexture(i) };
        fbDesc.extent = extent;
        fbDesc.layers = 1;
        m_taaFramebuffers[i] = device->createFramebuffer(fbDesc);
    }

    auto writeDescSet = [&](ASH::DescriptorSet* set, ASH::Texture* texture, ASH::Sampler* sampler) {
        ASH::DescriptorImageInfo info{ texture, sampler };
        ASH::DescriptorWrite write{ 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &info };
        set->update(&write, 1);
    };

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        ASH::Texture* srcTex = (level == 0) ? m_brightTarget.getColorTexture() : m_bloomChain.getMipTexture(level - 1);
        ASH::Sampler* srcSampler = (level == 0) ? m_brightTarget.getSampler() : m_bloomChain.getSampler();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_downsampleDescriptorSets[level][i].get(), srcTex, srcSampler);
    }

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT - 1; level++)
    {
        ASH::Texture* srcTex = m_bloomChain.getMipTexture(level + 1);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_upsampleDescriptorSets[level][i].get(), srcTex, m_bloomChain.getSampler());
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayMaskDescriptorSets[i].get(), m_depthBuffer.getTexture(), m_depthBuffer.getSampler());

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayBlurDescriptorSets[i].get(), m_godRayMaskTarget.getColorTexture(), m_godRayMaskTarget.getSampler());

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayCompositeDescriptorSets[i].get(), m_godRayBlurTarget.getColorTexture(), m_godRayBlurTarget.getSampler());

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        uint32_t prevSlot = 1 - slot;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            ASH::DescriptorImageInfo hdrInfo{ m_hdrTarget.getColorTexture(), m_hdrTarget.getSampler() };
            ASH::DescriptorImageInfo velInfo{ m_velocityTarget.getColorTexture(), m_velocityTarget.getSampler() };
            ASH::DescriptorImageInfo histInfo{ m_historyBuffer.getTexture(prevSlot), m_historyBuffer.getSampler() };

            std::array<ASH::DescriptorWrite, 3> writes{};
            writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &hdrInfo };
            writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &velInfo };
            writes[2] = { 2, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &histInfo };

            m_taaDescriptorSets[slot][i]->update(writes.data(), (uint32_t)writes.size());
        }
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_brightDescriptorSetsRaw[i].get(), m_hdrTarget.getColorTexture(), m_hdrTarget.getSampler());

    for (uint32_t slot = 0; slot < 2; slot++)
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_brightDescriptorSets[slot][i].get(), m_historyBuffer.getTexture(slot), m_historyBuffer.getSampler());

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            ASH::DescriptorImageInfo hdrInfo{ m_historyBuffer.getTexture(slot), m_historyBuffer.getSampler() };
            ASH::DescriptorImageInfo bloomInfo{ m_bloomChain.getMipTexture(0), m_bloomChain.getSampler() };
            std::array<ASH::DescriptorWrite, 2> writes{};
            writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &hdrInfo };
            writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &bloomInfo };
            m_postDescriptorSets[slot][i]->update(writes.data(), (uint32_t)writes.size());
        }
    }

    {
        ASH::DescriptorImageInfo hdrInfo{ m_hdrTarget.getColorTexture(), m_hdrTarget.getSampler() };
        ASH::DescriptorImageInfo bloomInfo{ m_bloomChain.getMipTexture(0), m_bloomChain.getSampler() };
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            std::array<ASH::DescriptorWrite, 2> writes{};
            writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &hdrInfo };
            writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &bloomInfo };
            m_postDescriptorSetsRaw[i]->update(writes.data(), (uint32_t)writes.size());
        }
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        m_fxaa.updateDescriptorSet(i, m_ldrTarget.getColorTexture(), m_ldrTarget.getSampler());

    uint32_t imageCount = m_swapchain.getImageCount();
    if (m_renderFinishedSemaphore.size() != imageCount)
    {
        m_renderFinishedSemaphore.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; i++)
            m_renderFinishedSemaphore[i] = device->createSemaphore();
        m_imagesInFlight.assign(imageCount, nullptr);
    }
}

void Renderer::setAAMethod(AAMethod method)
{
    m_aaMethod = method;

    switch (method)
    {
        case AAMethod::FXAA:
            break;
        case AAMethod::None:
        default:
            m_antiAliasing = std::make_unique<NoAA>();
            m_antiAliasing->initialize(m_context.getDevice(), m_swapchain.getExtent());
            break;
    }
}

void Renderer::cleanup()
{
    m_taaDescriptorSets[0].clear();
    m_taaDescriptorSets[1].clear();
    m_brightDescriptorSets[0].clear();
    m_brightDescriptorSets[1].clear();
    m_brightDescriptorSetsRaw.clear();
    for (auto& v : m_downsampleDescriptorSets) v.clear();
    for (auto& v : m_upsampleDescriptorSets) v.clear();
    m_godRayMaskDescriptorSets.clear();
    m_godRayBlurDescriptorSets.clear();
    m_godRayCompositeDescriptorSets.clear();
    m_postDescriptorSets[0].clear();
    m_postDescriptorSets[1].clear();
    m_postDescriptorSetsRaw.clear();
    m_fogDescriptorSets.clear();

    m_skyboxDescriptorSet.reset();
    m_skyboxPipeline.cleanup();
    m_environmentCubemap.cleanup();
    m_envBaker.cleanup();
    
    if (m_cleaned) return;
    m_cleaned = true;

    cleanupImGui();
    m_imguiDescriptorPool.reset();

    ASH::Device* device = m_context.getDevice();
    device->waitIdle();

    m_skinnedPipeline.cleanup();

    m_skyboxPipeline.cleanup();
    m_environmentCubemap.cleanup();
    m_irradianceCubemap.cleanup();
    m_envBaker.cleanup();

    m_voxelGridBuffer.reset();

    m_voxelGI.cleanup();

    m_godRayMaskFramebuffer.reset();
    m_godRayBlurFramebuffer.reset();
    m_godRayMaskPipeline.cleanup();
    m_godRayBlurPipeline.cleanup();
    m_godRayCompositePipeline.cleanup();
    m_godRayMaskTarget.cleanup();
    m_godRayBlurTarget.cleanup();

    m_decalPipeline.cleanup();
    m_fxaa.cleanup();
    m_ldrFramebuffer.reset();
    m_ldrRenderPass.cleanup();
    m_ldrTarget.cleanup();

    m_fogPipeline.cleanup();
    m_fogCameraBuffer.cleanup();

    m_hdrFramebuffer.reset();
    m_postPipeline.cleanup();
    m_hdrRenderPass.cleanup();
    m_hdrTarget.cleanup();

    m_velocityTarget.cleanup();

    m_decalBox.cleanup();
    m_decalCameraBuffer.cleanup();
    m_hdrDecalFramebuffer.reset();
    m_hdrDecalRenderPass.cleanup();

    m_pointShadowPipeline.cleanup();
    m_pointShadowRenderPass.cleanup();
    m_shadowAtlasFramebuffer.reset();
    m_shadowAtlas.cleanup();

    for (auto& decal : m_screenSpaceDecals)
        decal->cleanup();
    m_screenSpaceDecals.clear();

    for (auto& fb : m_taaFramebuffers) fb.reset();
    m_taaPipeline.cleanup();
    m_historyBuffer.cleanup();

    for (auto& fb : m_bloomFramebuffers) fb.reset();
    m_upsamplePipeline.cleanup();
    m_downsamplePipeline.cleanup();
    m_bloomChain.cleanup();

    m_brightFramebuffer.reset();
    m_brightPipeline.cleanup();
    m_brightTarget.cleanup();

    m_postRenderPassLoad.cleanup();
    m_postRenderPass.cleanup();

    m_shadowFramebuffer.reset();
    m_shadowPipeline.cleanup();
    m_shadowRenderPass.cleanup();
    m_shadowMap.cleanup();

    m_lightBuffer.cleanup();
    m_uniformBuffer.cleanup();
    m_imagesInFlight.clear();
    m_inFlightFences.clear();
    m_renderFinishedSemaphore.clear();
    m_imageAvailableSemaphore.clear();
    m_commandManager.cleanup();
    m_framebuffers.cleanup();
    m_pipeline.cleanup();
    m_renderPass.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.cleanup();
    m_context.shutdown();
}
std::array<glm::vec4, 6> Renderer::extractFrustumPlanes(const glm::mat4& viewProj) const
{
    std::array<glm::vec4, 6> planes;
    glm::mat4 m = glm::transpose(viewProj);

    planes[0] = m[3] + m[0];
    planes[1] = m[3] - m[0];
    planes[2] = m[3] + m[1];
    planes[3] = m[3] - m[1];
    planes[4] = m[3] + m[2];
    planes[5] = m[3] - m[2];

    for (auto& p : planes)
        p /= glm::length(glm::vec3(p));

    return planes;
}

bool Renderer::isSubmeshVisible(const glm::mat4& modelMatrix, const Submesh& sm, const std::array<glm::vec4, 6>& planes) const
{
    glm::vec3 corners[8] = {
        { sm.boundsMin.x, sm.boundsMin.y, sm.boundsMin.z },
        { sm.boundsMax.x, sm.boundsMin.y, sm.boundsMin.z },
        { sm.boundsMin.x, sm.boundsMax.y, sm.boundsMin.z },
        { sm.boundsMax.x, sm.boundsMax.y, sm.boundsMin.z },
        { sm.boundsMin.x, sm.boundsMin.y, sm.boundsMax.z },
        { sm.boundsMax.x, sm.boundsMin.y, sm.boundsMax.z },
        { sm.boundsMin.x, sm.boundsMax.y, sm.boundsMax.z },
        { sm.boundsMax.x, sm.boundsMax.y, sm.boundsMax.z }
    };

    glm::vec3 worldCorners[8];
    for (int i = 0; i < 8; i++)
        worldCorners[i] = glm::vec3(modelMatrix * glm::vec4(corners[i], 1.0f));

    for (const auto& plane : planes)
    {
        int outsideCount = 0;
        for (int i = 0; i < 8; i++)
        {
            float dist = glm::dot(glm::vec3(plane), worldCorners[i]) + plane.w;
            if (dist < 0.0f)
                outsideCount++;
        }
        if (outsideCount == 8)
            return false;
    }

    return true;
}

void Renderer::updatePointLightMatrices(int slot, glm::vec3 lightPos)
{
    float aspect = 1.0f;
    float nearPlane = 0.1f;
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect, nearPlane, m_pointShadowFarPlane);
    m_pointLightViewProjArr[slot][0] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0));
    m_pointLightViewProjArr[slot][1] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0));
    m_pointLightViewProjArr[slot][2] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1));
    m_pointLightViewProjArr[slot][3] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1));
    m_pointLightViewProjArr[slot][4] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0));
    m_pointLightViewProjArr[slot][5] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0));
}

bool Renderer::isVisible(const GameObject& obj, const std::array<glm::vec4, 6>& planes) const
{
    if (!obj.mesh) return false;

    glm::vec3 worldCentre = glm::vec3(obj.transform.getMatrix() * glm::vec4(obj.mesh->getLocalBoundCentre(), 1.0f));

    float maxScale = glm::max(obj.transform.scale.x, glm::max(obj.transform.scale.y, obj.transform.scale.z));
    float worldRadius = obj.mesh->getLocalBoundsRadius() * maxScale;

    for (const auto& plane : planes)
    {
        float dist = glm::dot(glm::vec3(plane), worldCentre) + plane.w;

        if (dist < -worldRadius)
            return false;
    }

    return true;
}

std::vector<const GameObject*> Renderer::parallelCull(const Scene& scene, const std::array<glm::vec4, 6>& planes) const
{
    const auto& objects = scene.getObjects();
    size_t total = objects.size();

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    numThreads = std::min((size_t)numThreads, total > 0 ? total : 1);

    size_t chunkSize = (total + numThreads - 1) / numThreads;

    std::vector<std::future<std::vector<const GameObject*>>> futures;

    for (unsigned int t = 0; t < numThreads; t++)
    {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, total);
        if (start >= end) continue;

        futures.push_back(std::async(std::launch::async, [this, &objects, &planes, start, end]() {
            std::vector<const GameObject*> localVisible;
            for (size_t i = start; i < end; i++)
            {
                if (isVisible(*objects[i], planes))
                    localVisible.push_back(objects[i].get());
            }
            return localVisible;
        }));
    }

    std::vector<const GameObject*> visible;
    for (auto& f : futures)
    {
        auto chunk = f.get();
        visible.insert(visible.end(), chunk.begin(), chunk.end());
    }

    return visible;
}

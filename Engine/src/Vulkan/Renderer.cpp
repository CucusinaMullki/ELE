#include "Vulkan/Renderer.h"
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
    m_imguiDescriptorPool = m_context.getDevice().createDescriptorPoolUnique(poolInfo);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    uint32_t swapchianImageCount = m_swapchain.getImageCount();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_context.getInstance();
    initInfo.PhysicalDevice = m_context.getPhysicalDevice();
    initInfo.Device = m_context.getDevice();
    initInfo.QueueFamily = m_context.getGraphicQueueFamily();
    initInfo.Queue = m_context.getGraphicQueue();
    initInfo.DescriptorPool = m_imguiDescriptorPool.get();
    initInfo.RenderPass = m_renderPass.getRenderPass();
    initInfo.MinImageCount = swapchianImageCount;
    initInfo.ImageCount = swapchianImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void Renderer::beginImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Renderer::renderImGui(vk::CommandBuffer commandBuffer)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
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
    LOG("swapchain extent: " << m_swapchain.getExtent().width << "x" << m_swapchain.getExtent().height);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    LOG("depth buffer created with extent from swapchain");
    m_renderPass.initialize(m_context.getDevice(), m_swapchain.getImageFormat(), m_depthBuffer.getFormat());
    m_pipelineCache.initialize(m_context.getDevice());

    setAAMethod(AAMethod::None);

    m_hdrTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_velocityTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_hdrRenderPass.initialize(m_context.getDevice(), m_hdrTarget.getColorFormat(), m_velocityTarget.getColorFormat(), m_depthBuffer.getFormat());

    std::array<vk::ImageView, 3> hdrAttachment = {
        m_hdrTarget.getColorView(),
        m_velocityTarget.getColorView(),
        m_depthBuffer.getImageView()
    };
    vk::FramebufferCreateInfo hdrFbInfo{};
    hdrFbInfo.setRenderPass(m_hdrRenderPass.getRenderPass());
    hdrFbInfo.setAttachments(hdrAttachment);
    hdrFbInfo.setWidth(m_swapchain.getExtent().width);
    hdrFbInfo.setHeight(m_swapchain.getExtent().height);
    hdrFbInfo.setLayers(1);
    m_hdrFramebuffer = m_context.getDevice().createFramebufferUnique(hdrFbInfo);

    LOG("HDR fb depth view = " << (void*)(VkImageView)m_depthBuffer.getImageView());

    m_hdrDecalRenderPass.initialize(m_context.getDevice(), m_hdrTarget.getColorFormat(), vk::AttachmentLoadOp::eLoad);

    vk::ImageView hdrDecalView = m_hdrTarget.getColorView();
    vk::FramebufferCreateInfo hdrDecalFbInfo{};
    hdrDecalFbInfo.setRenderPass(m_hdrDecalRenderPass.getRenderPass());
    hdrDecalFbInfo.setAttachmentCount(1);
    hdrDecalFbInfo.setPAttachments(&hdrDecalView);
    hdrDecalFbInfo.setWidth(m_swapchain.getExtent().width);
    hdrDecalFbInfo.setHeight(m_swapchain.getExtent().height);
    hdrDecalFbInfo.setLayers(1);
    m_hdrDecalFramebuffer = m_context.getDevice().createFramebufferUnique(hdrDecalFbInfo);

    vk::Extent2D bloomExtent = m_swapchain.getExtent();
    m_brighTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), bloomExtent);
    m_postRenderPass.initialize(m_context.getDevice(), m_brighTarget.getColorFormat());

    m_historyBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());

    for (uint32_t i = 0; i < 2; i++)
    {
        vk::ImageView view = m_historyBuffer.getView(i);
        vk::FramebufferCreateInfo fbInfo{};
        fbInfo.setRenderPass(m_postRenderPass.getRenderPass());
        fbInfo.setAttachmentCount(1);
        fbInfo.setPAttachments(&view);
        fbInfo.setWidth(m_swapchain.getExtent().width);
        fbInfo.setHeight(m_swapchain.getExtent().height);
        fbInfo.setLayers(1);
        m_taaFramebuffers[i] = m_context.getDevice().createFramebufferUnique(fbInfo);
    }

    PipelineConfig taaConfig{};
    taaConfig.vertexShader = "fullscreen";
    taaConfig.fragmentShader = "taa";
    taaConfig.renderPass = m_postRenderPass.getRenderPass();
    taaConfig.cache = m_pipelineCache.get();
    taaConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    taaConfig.postProcess = true;
    taaConfig.useDescriptors = false;
    taaConfig.useVertexInput = false;
    taaConfig.depthTest = false;
    taaConfig.depthWrite = false;
    taaConfig.cullMode = vk::CullModeFlagBits::eNone;
    taaConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    taaConfig.pushConstantSize = sizeof(float);
    m_taaPipeline.initialize(m_context.getDevice(), taaConfig);

    auto allocTaaDescSets = [&]() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_taaPipeline.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.setDescriptorPool(m_taaPipeline.getDescriptorPool());
        allocInfo.setSetLayouts(layouts);
        return m_context.getDevice().allocateDescriptorSets(allocInfo);
    };

    for (uint32_t slot = 0; slot < 2; slot++)
        m_taaDescriptorSets[slot] = allocTaaDescSets();

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        uint32_t prevSlot = 1 - slot;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            std::array<vk::DescriptorImageInfo, 3> infos{};
            infos[0].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            infos[0].setImageView(m_hdrTarget.getColorView());
            infos[0].setSampler(m_hdrTarget.getSampler());

            infos[1].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            infos[1].setImageView(m_velocityTarget.getColorView());
            infos[1].setSampler(m_velocityTarget.getSampler());

            infos[2].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            infos[2].setImageView(m_historyBuffer.getView(prevSlot));
            infos[2].setSampler(m_historyBuffer.getSampler());

            std::array<vk::WriteDescriptorSet, 3> writes{};
            for (uint32_t b = 0; b < 3; b++)
            {
                writes[b].setDstSet(m_taaDescriptorSets[slot][i]);
                writes[b].setDstBinding(b);
                writes[b].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                writes[b].setDescriptorCount(1);
                writes[b].setImageInfo(infos[b]);
            }
            m_context.getDevice().updateDescriptorSets(writes, nullptr);
        }
    }

    vk::ImageView brightView = m_brighTarget.getColorView();
    vk::FramebufferCreateInfo brightFbInfo{};
    brightFbInfo.setRenderPass(m_postRenderPass.getRenderPass());
    brightFbInfo.setAttachmentCount(1);
    brightFbInfo.setPAttachments(&brightView);
    brightFbInfo.setWidth(bloomExtent.width);
    brightFbInfo.setHeight(bloomExtent.height);
    brightFbInfo.setLayers(1);
    m_brightFramebuffer = m_context.getDevice().createFramebufferUnique(brightFbInfo);

    PipelineConfig brightConfig{};
    brightConfig.vertexShader = "fullscreen";
    brightConfig.fragmentShader = "brightpass";
    brightConfig.renderPass = m_postRenderPass.getRenderPass();
    brightConfig.cache = m_pipelineCache.get();
    brightConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    brightConfig.postProcess = true;
    brightConfig.useDescriptors = false;
    brightConfig.useVertexInput = false;
    brightConfig.depthTest = false;
    brightConfig.depthWrite = false;
    brightConfig.cullMode = vk::CullModeFlagBits::eNone;
    brightConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    brightConfig.pushConstantSize = sizeof(float);
    m_brightPipeline.initialize(m_context.getDevice(), brightConfig);

    auto allocBrightDescSets = [&]() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_brightPipeline.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.setDescriptorPool(m_brightPipeline.getDescriptorPool());
        allocInfo.setSetLayouts(layouts);
        return m_context.getDevice().allocateDescriptorSets(allocInfo);
    };

    m_brightDescriptorSets[0] = allocBrightDescSets();
    m_brightDescriptorSets[1] = allocBrightDescSets();

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorImageInfo info{};
            info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            info.setImageView(m_historyBuffer.getView(slot));
            info.setSampler(m_historyBuffer.getSampler());

            vk::WriteDescriptorSet write{};
            write.setDstSet(m_brightDescriptorSets[slot][i]);
            write.setDstBinding(0);
            write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            write.setDescriptorCount(1);
            write.setImageInfo(info);
            m_context.getDevice().updateDescriptorSets(write, nullptr);
        }
    }

    m_brightDescriptorSetsRaw = allocBrightDescSets();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorImageInfo info{};
        info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        info.setImageView(m_hdrTarget.getColorView());
        info.setSampler(m_hdrTarget.getSampler());

        vk::WriteDescriptorSet write{};
        write.setDstSet(m_brightDescriptorSetsRaw[i]);
        write.setDstBinding(0);
        write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        write.setDescriptorCount(1);
        write.setImageInfo(info);
        m_context.getDevice().updateDescriptorSets(write, nullptr);
    }

    m_bloomChain.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), bloomExtent);

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        vk::ImageView view = m_bloomChain.getMipView(level);
        vk::FramebufferCreateInfo fbInfo{};
        fbInfo.setRenderPass(m_postRenderPass.getRenderPass());
        fbInfo.setAttachmentCount(1);
        fbInfo.setPAttachments(&view);
        vk::Extent2D mipExtent = m_bloomChain.getMipExtent(level);
        fbInfo.setWidth(mipExtent.width);
        fbInfo.setHeight(mipExtent.height);
        fbInfo.setLayers(1);
        m_bloomFramebuffers[level] = m_context.getDevice().createFramebufferUnique(fbInfo);
    }

    PipelineConfig downsampleConfig{};
    downsampleConfig.vertexShader = "fullscreen";
    downsampleConfig.fragmentShader = "downsample";
    downsampleConfig.renderPass = m_postRenderPass.getRenderPass();
    downsampleConfig.cache = m_pipelineCache.get();
    downsampleConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    downsampleConfig.postProcess = true;
    downsampleConfig.useDescriptors = false;
    downsampleConfig.useVertexInput = false;
    downsampleConfig.depthTest = false;
    downsampleConfig.depthWrite = false;
    downsampleConfig.cullMode = vk::CullModeFlagBits::eNone;
    downsampleConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    downsampleConfig.pushConstantSize = sizeof(glm::vec2);
    m_downsamplePipeline.initialize(m_context.getDevice(), downsampleConfig);

    PipelineConfig upsampleConfig = downsampleConfig;
    upsampleConfig.fragmentShader = "upsample";
    upsampleConfig.pushConstantSize = sizeof(float);
    upsampleConfig.blendEnable = true;
    upsampleConfig.blendSrcFactor = vk::BlendFactor::eOne;
    upsampleConfig.blendDstFactor = vk::BlendFactor::eOne;
    upsampleConfig.blendOp = vk::BlendOp::eAdd;
    m_upsamplePipeline.initialize(m_context.getDevice(), upsampleConfig);

    auto allocDescSets = [&](Pipeline& pipe) {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, pipe.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.setDescriptorPool(pipe.getDescriptorPool());
        allocInfo.setSetLayouts(layouts);
        return m_context.getDevice().allocateDescriptorSets(allocInfo);
    };

    auto writeDescSet = [&](vk::DescriptorSet set, vk::ImageView view, vk::Sampler sampler) {
        vk::DescriptorImageInfo info{};
        info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        info.setImageView(view);
        info.setSampler(sampler);

        vk::WriteDescriptorSet write{};
        write.setDstSet(set);
        write.setDstBinding(0);
        write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        write.setDescriptorCount(1);
        write.setImageInfo(info);
        m_context.getDevice().updateDescriptorSets(write, nullptr);
    };

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        m_downsampleDescriptorSets[level] = allocDescSets(m_downsamplePipeline);
        vk::ImageView srcView = (level == 0) ? m_brighTarget.getColorView() : m_bloomChain.getMipView(level - 1);
        vk::Sampler srcSampler = (level == 0) ? m_brighTarget.getSampler() : m_bloomChain.getSampler();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_downsampleDescriptorSets[level][i], srcView, srcSampler);
    }

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT - 1; level++)
    {
        m_upsampleDescriptorSets[level] = allocDescSets(m_upsamplePipeline);
        vk::ImageView srcView = m_bloomChain.getMipView(level + 1);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_upsampleDescriptorSets[level][i], srcView, m_bloomChain.getSampler());
    }

    m_godRayMaskTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_godRayBlurTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());

    vk::ImageView maskView = m_godRayMaskTarget.getColorView();
    vk::FramebufferCreateInfo maskFbInfo{};
    maskFbInfo.setRenderPass(m_postRenderPass.getRenderPass());
    maskFbInfo.setAttachmentCount(1);
    maskFbInfo.setPAttachments(&maskView);
    maskFbInfo.setWidth(bloomExtent.width);
    maskFbInfo.setHeight(bloomExtent.height);
    maskFbInfo.setLayers(1);
    m_godRayMaskFramebuffer = m_context.getDevice().createFramebufferUnique(maskFbInfo);

    vk::ImageView blurView = m_godRayBlurTarget.getColorView();
    vk::FramebufferCreateInfo blurFbInfo{};
    blurFbInfo.setRenderPass(m_postRenderPass.getRenderPass());
    blurFbInfo.setAttachmentCount(1);
    blurFbInfo.setPAttachments(&blurView);
    blurFbInfo.setWidth(bloomExtent.width);
    blurFbInfo.setHeight(bloomExtent.height);
    blurFbInfo.setLayers(1);
    m_godRayBlurFramebuffer = m_context.getDevice().createFramebufferUnique(blurFbInfo);

    PipelineConfig godRayMaskConfig{};
    godRayMaskConfig.vertexShader = "fullscreen";
    godRayMaskConfig.fragmentShader = "godray_mask";
    godRayMaskConfig.renderPass = m_postRenderPass.getRenderPass();
    godRayMaskConfig.cache = m_pipelineCache.get();
    godRayMaskConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    godRayMaskConfig.postProcess = true;
    godRayMaskConfig.useDescriptors = false;
    godRayMaskConfig.useVertexInput = false;
    godRayMaskConfig.depthTest = false;
    godRayMaskConfig.depthWrite = false;
    godRayMaskConfig.cullMode = vk::CullModeFlagBits::eNone;
    godRayMaskConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    godRayMaskConfig.pushConstantSize = sizeof(glm::mat4) + sizeof(glm::vec4) * 2;
    m_godRayMaskPipeline.initialize(m_context.getDevice(), godRayMaskConfig);

    PipelineConfig godRayBlurConfig{};
    godRayBlurConfig.vertexShader = "fullscreen";
    godRayBlurConfig.fragmentShader = "godray_blur";
    godRayBlurConfig.renderPass = m_postRenderPass.getRenderPass();
    godRayBlurConfig.cache = m_pipelineCache.get();
    godRayBlurConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    godRayBlurConfig.postProcess = true;
    godRayBlurConfig.useDescriptors = false;
    godRayBlurConfig.useVertexInput = false;
    godRayBlurConfig.depthTest = false;
    godRayBlurConfig.depthWrite = false;
    godRayBlurConfig.cullMode = vk::CullModeFlagBits::eNone;
    godRayBlurConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    godRayBlurConfig.pushConstantSize = sizeof(glm::vec4) * 2;
    m_godRayBlurPipeline.initialize(m_context.getDevice(), godRayBlurConfig);

    PipelineConfig godRayCompositeConfig{};
    godRayCompositeConfig.vertexShader = "fullscreen";
    godRayCompositeConfig.fragmentShader = "godray_composite";
    godRayCompositeConfig.renderPass = m_hdrDecalRenderPass.getRenderPass();
    godRayCompositeConfig.cache = m_pipelineCache.get();
    godRayCompositeConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    godRayCompositeConfig.postProcess = true;
    godRayCompositeConfig.useDescriptors = false;
    godRayCompositeConfig.useVertexInput = false;
    godRayCompositeConfig.colorAttachmentCount = 1;
    godRayCompositeConfig.depthTest = false;
    godRayCompositeConfig.depthWrite = false;
    godRayCompositeConfig.cullMode = vk::CullModeFlagBits::eNone;
    godRayCompositeConfig.blendEnable = true;
    godRayCompositeConfig.blendSrcFactor = vk::BlendFactor::eOne;
    godRayCompositeConfig.blendDstFactor = vk::BlendFactor::eOne;
    godRayCompositeConfig.blendOp = vk::BlendOp::eAdd;
    godRayCompositeConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    godRayCompositeConfig.pushConstantSize = sizeof(float);
    m_godRayCompositePipeline.initialize(m_context.getDevice(), godRayCompositeConfig);

    m_godRayMaskDescriptorSets = allocDescSets(m_godRayMaskPipeline);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayMaskDescriptorSets[i], m_depthBuffer.getImageView(), m_depthBuffer.getSampler());

    m_godRayBlurDescriptorSets = allocDescSets(m_godRayBlurPipeline);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayBlurDescriptorSets[i], m_godRayMaskTarget.getColorView(), m_godRayMaskTarget.getSampler());

    m_godRayCompositeDescriptorSets = allocDescSets(m_godRayCompositePipeline);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayCompositeDescriptorSets[i], m_godRayBlurTarget.getColorView(), m_godRayBlurTarget.getSampler());

    m_postRenderPassLoad.initialize(m_context.getDevice(), m_brighTarget.getColorFormat(), vk::AttachmentLoadOp::eLoad);

    m_ldrTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_ldrRenderPass.initialize(m_context.getDevice(), m_ldrTarget.getColorFormat());

    vk::ImageView ldrView = m_ldrTarget.getColorView();
    vk::FramebufferCreateInfo ldrFbInfo{};
    ldrFbInfo.setRenderPass(m_ldrRenderPass.getRenderPass());
    ldrFbInfo.setAttachmentCount(1);
    ldrFbInfo.setPAttachments(&ldrView);
    ldrFbInfo.setWidth(m_swapchain.getExtent().width);
    ldrFbInfo.setHeight(m_swapchain.getExtent().height);
    ldrFbInfo.setLayers(1);
    m_ldrFramebuffer = m_context.getDevice().createFramebufferUnique(ldrFbInfo);

    PipelineConfig postConfig{};
    postConfig.vertexShader = "fullscreen";
    postConfig.fragmentShader = "tonemap";
    postConfig.renderPass = m_ldrRenderPass.getRenderPass();
    postConfig.cache = m_pipelineCache.get();
    postConfig.useVertexInput = false;
    postConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    postConfig.pushStage = vk::ShaderStageFlagBits::eFragment;
    postConfig.pushConstantSize = sizeof(float) * 2;
    postConfig.postProcess = true;
    postConfig.useDescriptors = false;
    postConfig.colorAttachmentCount = 1;
    postConfig.depthTest = false;
    postConfig.depthWrite = false;
    postConfig.cullMode = vk::CullModeFlagBits::eNone;
    m_postPipeline.initialize(m_context.getDevice(), postConfig);

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        std::vector<vk::DescriptorSetLayout> postLayouts(MAX_FRAMES_IN_FLIGHT, m_postPipeline.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo postAlloc{};
        postAlloc.setDescriptorPool(m_postPipeline.getDescriptorPool());
        postAlloc.setSetLayouts(postLayouts);
        m_postDescriptorSets[slot] = m_context.getDevice().allocateDescriptorSets(postAlloc);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorImageInfo hdrInfo{};
            hdrInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            hdrInfo.setImageView(m_historyBuffer.getView(slot));
            hdrInfo.setSampler(m_historyBuffer.getSampler());

            vk::DescriptorImageInfo bloomInfo{};
            bloomInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            bloomInfo.setImageView(m_bloomChain.getMipView(0));
            bloomInfo.setSampler(m_bloomChain.getSampler());

            std::array<vk::WriteDescriptorSet, 2> writes{};
            writes[0].setDstSet(m_postDescriptorSets[slot][i]);
            writes[0].setDstBinding(0);
            writes[0].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[0].setDescriptorCount(1);
            writes[0].setImageInfo(hdrInfo);

            writes[1].setDstSet(m_postDescriptorSets[slot][i]);
            writes[1].setDstBinding(1);
            writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[1].setDescriptorCount(1);
            writes[1].setImageInfo(bloomInfo);

            m_context.getDevice().updateDescriptorSets(writes, nullptr);
        }
    }

    {
        std::vector<vk::DescriptorSetLayout> postLayoutsRaw(MAX_FRAMES_IN_FLIGHT, m_postPipeline.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo postAllocRaw{};
        postAllocRaw.setDescriptorPool(m_postPipeline.getDescriptorPool());
        postAllocRaw.setSetLayouts(postLayoutsRaw);
        m_postDescriptorSetsRaw = m_context.getDevice().allocateDescriptorSets(postAllocRaw);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorImageInfo hdrInfo{};
            hdrInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            hdrInfo.setImageView(m_hdrTarget.getColorView());
            hdrInfo.setSampler(m_hdrTarget.getSampler());

            vk::DescriptorImageInfo bloomInfo{};
            bloomInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            bloomInfo.setImageView(m_bloomChain.getMipView(0));
            bloomInfo.setSampler(m_bloomChain.getSampler());

            std::array<vk::WriteDescriptorSet, 2> writes{};
            writes[0].setDstSet(m_postDescriptorSetsRaw[i]);
            writes[0].setDstBinding(0);
            writes[0].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[0].setDescriptorCount(1);
            writes[0].setImageInfo(hdrInfo);

            writes[1].setDstSet(m_postDescriptorSetsRaw[i]);
            writes[1].setDstBinding(1);
            writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[1].setDescriptorCount(1);
            writes[1].setImageInfo(bloomInfo);

            m_context.getDevice().updateDescriptorSets(writes, nullptr);
        }
    }

    m_fxaa.initializeWithRenderPass(m_context.getDevice(), m_pipelineCache.get(),
        m_renderPass.getRenderPass(), MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        m_fxaa.updateDescriptorSet(m_context.getDevice(), i, m_ldrTarget.getColorView(), m_ldrTarget.getSampler());

    PipelineConfig pbrConfig{};
    pbrConfig.vertexShader = "pbr";
    pbrConfig.fragmentShader = "pbr";
    pbrConfig.renderPass = m_hdrRenderPass.getRenderPass();
    pbrConfig.cache = m_pipelineCache.get();
    pbrConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    pbrConfig.useDescriptors = true;
    pbrConfig.colorAttachmentCount = 2;
    pbrConfig.cullMode = vk::CullModeFlagBits::eBack;
    pbrConfig.pushConstantSize = sizeof(glm::mat4) * 3;
    m_pipeline.initialize(m_context.getDevice(), pbrConfig);

    PipelineConfig decalConfig{};
    decalConfig.vertexShader = "decal_box";
    decalConfig.fragmentShader = "decal_box";
    decalConfig.renderPass = m_hdrDecalRenderPass.getRenderPass();
    decalConfig.cache = m_pipelineCache.get();
    decalConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    decalConfig.decalLayout = true;
    decalConfig.useDescriptors = false;
    decalConfig.colorAttachmentCount = 1;
    decalConfig.pushConstantSize = sizeof(glm::mat4) * 2 + sizeof(glm::vec4);
    decalConfig.pushStage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    decalConfig.blendEnable = true;
    decalConfig.blendSrcFactor = vk::BlendFactor::eSrcAlpha;
    decalConfig.blendDstFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    decalConfig.blendOp = vk::BlendOp::eAdd;
    decalConfig.cullMode = vk::CullModeFlagBits::eFront;
    decalConfig.depthTest = false;
    decalConfig.depthWrite = false;
    decalConfig.useVertexInput = true;
    m_decalBox.initialize(m_context.getDevice(), m_context.getPhysicalDevice());
    m_decalCameraBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);
    m_decalPipeline.initialize(m_context.getDevice(), decalConfig);

    m_lightBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);

    auto indices = m_context.GetQueueFamilyIndices();
    m_commandManager.intialize(m_context.getDevice(), indices.graphicFamily.value(), MAX_FRAMES_IN_FLIGHT);

    m_uniformBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    m_shadowMap.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), 2048);
    m_shadowRenderPass.initialize(m_context.getDevice(), m_shadowMap.getFormat());

    vk::ImageView shadowAttachment = m_shadowMap.getImageView();
    vk::FramebufferCreateInfo shadowFbInfo{};
    shadowFbInfo.setRenderPass(m_shadowRenderPass.getRenderPass());
    shadowFbInfo.setAttachmentCount(1);
    shadowFbInfo.setPAttachments(&shadowAttachment);
    shadowFbInfo.setWidth(m_shadowMap.getSize());
    shadowFbInfo.setHeight(m_shadowMap.getSize());
    shadowFbInfo.setLayers(1);
    m_shadowFramebuffer = m_context.getDevice().createFramebufferUnique(shadowFbInfo);

    PipelineConfig shadowConfig{};
    shadowConfig.vertexShader = "shadow";
    shadowConfig.fragmentShader = "";
    shadowConfig.renderPass = m_shadowRenderPass.getRenderPass();
    shadowConfig.cache = m_pipelineCache.get();
    shadowConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    shadowConfig.useDescriptors = false;
    shadowConfig.colorAttachmentCount = 0;
    shadowConfig.cullMode = vk::CullModeFlagBits::eFront;
    shadowConfig.depthBiasEnable = true;
    shadowConfig.depthBiasConstant = 1.25f;
    shadowConfig.depthBiasSlope = 1.75f;
    m_shadowPipeline.initialize(m_context.getDevice(), shadowConfig);

    m_pointShadowRenderPass.initialize(m_context.getDevice(), vk::Format::eD32Sfloat);

    m_shadowAtlas.initialize(m_context.getDevice(), m_context.getPhysicalDevice());

    vk::ImageView atlasView = m_shadowAtlas.getImageView();
    vk::FramebufferCreateInfo fbInfo{};
    fbInfo.setRenderPass(m_pointShadowRenderPass.getRenderPass());
    fbInfo.setAttachmentCount(1);
    fbInfo.setPAttachments(&atlasView);
    fbInfo.setWidth(m_shadowAtlas.getWidth());
    fbInfo.setHeight(m_shadowAtlas.getHeight());
    fbInfo.setLayers(1);
    m_shadowAtlasFramebuffer = m_context.getDevice().createFramebufferUnique(fbInfo);

    PipelineConfig pointShadowConfig{};
    pointShadowConfig.vertexShader = "point_shadow";
    pointShadowConfig.fragmentShader = "point_shadow";
    pointShadowConfig.renderPass = m_pointShadowRenderPass.getRenderPass();
    pointShadowConfig.cache = m_pipelineCache.get();
    pointShadowConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    pointShadowConfig.useDescriptors = false;
    pointShadowConfig.colorAttachmentCount = 0;
    pointShadowConfig.cullMode = vk::CullModeFlagBits::eFront;
    pointShadowConfig.depthBiasEnable = true;
    pointShadowConfig.depthBiasConstant = 1.25f;
    pointShadowConfig.depthBiasSlope = 1.75f;
    pointShadowConfig.pushConstantSize = sizeof(glm::mat4) * 2 + sizeof(glm::vec4);
    pointShadowConfig.pushStage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    m_pointShadowPipeline.initialize(m_context.getDevice(), pointShadowConfig);

    m_fogCameraBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);

    PipelineConfig fogConfig{};
    fogConfig.vertexShader = "fullscreen";
    fogConfig.fragmentShader = "fog";
    fogConfig.renderPass = m_hdrDecalRenderPass.getRenderPass();
    fogConfig.cache = m_pipelineCache.get();
    fogConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    fogConfig.colorAttachmentCount = 1;
    fogConfig.useVertexInput = false;
    fogConfig.fogLayout = true;
    fogConfig.depthTest = false;
    fogConfig.depthWrite = false;
    fogConfig.cullMode = vk::CullModeFlagBits::eNone;
    fogConfig.blendEnable = true;
    fogConfig.blendSrcFactor = vk::BlendFactor::eSrcAlpha;
    fogConfig.blendDstFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    fogConfig.blendOp = vk::BlendOp::eAdd;
    fogConfig.pushConstantSize = sizeof(glm::vec4) * 4;
    fogConfig.pushStage = vk::ShaderStageFlagBits::eFragment;

    m_fogPipeline.initialize(m_context.getDevice(), fogConfig);
    m_context.getDevice().resetDescriptorPool(m_fogPipeline.getDescriptorPool());
    createFogDescriptorSets();

    size_t imageCount = m_swapchain.getImageViews().size();
    m_imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphore.resize(imageCount);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_imageAvailableSemaphore[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
        m_inFlightFences[i] = m_context.getDevice().createFenceUnique(fenceInfo);
    }
    for (size_t i = 0; i < imageCount; i++)
        m_renderFinishedSemaphore[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);

    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.setCommandPool(m_commandManager.getCommandPool());
        allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
        allocInfo.setCommandBufferCount(1);
        auto cmdBuffers = m_context.getDevice().allocateCommandBuffersUnique(allocInfo);
        auto& cmd = cmdBuffers[0];

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        cmd->begin(beginInfo);

        {
            std::array<vk::ClearValue, 3> hdrClear{};
            hdrClear[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
            hdrClear[1].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
            hdrClear[2].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });
            vk::RenderPassBeginInfo hdrPass{};
            hdrPass.setRenderPass(m_hdrRenderPass.getRenderPass());
            hdrPass.setFramebuffer(m_hdrFramebuffer.get());
            hdrPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
            hdrPass.setClearValues(hdrClear);
            cmd->beginRenderPass(hdrPass, vk::SubpassContents::eInline);
            cmd->endRenderPass();
        }

        for (uint32_t slot = 0; slot < 2; slot++)
        {
            vk::ClearValue clear{};
            clear.setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });

            vk::RenderPassBeginInfo pass{};
            pass.setRenderPass(m_postRenderPass.getRenderPass());
            pass.setFramebuffer(m_taaFramebuffers[slot].get());
            pass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
            pass.setClearValues(clear);

            cmd->beginRenderPass(pass, vk::SubpassContents::eInline);
            cmd->endRenderPass();
        }

        {
            vk::ClearValue shadowClear{};
            shadowClear.setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });
            
            vk::RenderPassBeginInfo atlasPass{};
            atlasPass.setRenderPass(m_pointShadowRenderPass.getRenderPass());
            atlasPass.setFramebuffer(m_shadowAtlasFramebuffer.get());
            atlasPass.setRenderArea(vk::Rect2D{ {0, 0}, { m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() } });
            atlasPass.setClearValues(shadowClear);

            cmd->beginRenderPass(atlasPass, vk::SubpassContents::eInline);
            cmd->endRenderPass();
        }

        {
            vk::ClearValue clear{};
            clear.setColor(vk::ClearColorValue{ std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f} });

            vk::RenderPassBeginInfo maskPass{};
            maskPass.setRenderPass(m_postRenderPass.getRenderPass());
            maskPass.setFramebuffer(m_godRayMaskFramebuffer.get());
            maskPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
            maskPass.setClearValues(clear);
            cmd->beginRenderPass(maskPass, vk::SubpassContents::eInline);
            cmd->endRenderPass();

            vk::RenderPassBeginInfo blurPass{};
            blurPass.setRenderPass(m_postRenderPass.getRenderPass());
            blurPass.setFramebuffer(m_godRayBlurFramebuffer.get());
            blurPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
            blurPass.setClearValues(clear);
            cmd->beginRenderPass(blurPass, vk::SubpassContents::eInline);
            cmd->endRenderPass();
        }

        cmd->end();

        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(cmd.get());
        m_context.getGraphicQueue().submit(submitInfo, nullptr);
        m_context.getGraphicQueue().waitIdle();
    }
}

void Renderer::createFogDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_fogPipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(m_fogPipeline.getDescriptorPool());
    allocInfo.setSetLayouts(layouts);
    m_fogDescriptorSets = m_context.getDevice().allocateDescriptorSets(allocInfo);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorImageInfo depthInfo{};
        depthInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        depthInfo.setImageView(m_depthBuffer.getImageView());
        depthInfo.setSampler(m_depthBuffer.getSampler());

        vk::DescriptorImageInfo shadowInfo{};
        shadowInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        shadowInfo.setImageView(m_shadowMap.getImageView());
        shadowInfo.setSampler(m_shadowMap.getSampler());

        vk::DescriptorBufferInfo camInfo   = m_fogCameraBuffer.getDescriptorInfo(i);
        vk::DescriptorBufferInfo lightInfo = m_lightBuffer.getDescriptorInfo(i);

        std::array<vk::WriteDescriptorSet, 4> writes{};
        writes[0].setDstSet(m_fogDescriptorSets[i]).setDstBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1).setImageInfo(depthInfo);
        writes[1].setDstSet(m_fogDescriptorSets[i]).setDstBinding(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setBufferInfo(camInfo);
        writes[2].setDstSet(m_fogDescriptorSets[i]).setDstBinding(2)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1).setImageInfo(shadowInfo);
        writes[3].setDstSet(m_fogDescriptorSets[i]).setDstBinding(3)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setBufferInfo(lightInfo);

        m_context.getDevice().updateDescriptorSets(writes, nullptr);
    }
}



std::vector<vk::DescriptorBufferInfo> Renderer::getLightInfos() const
{
    std::vector<vk::DescriptorBufferInfo> infos;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        infos.push_back(m_lightBuffer.getDescriptorInfo(i));
    return infos;
}

float Renderer::getAspect() const
{
    return (float)m_swapchain.getExtent().width / (float)m_swapchain.getExtent().height;
}

std::vector<vk::DescriptorBufferInfo> Renderer::getUboInfos() const
{
    std::vector<vk::DescriptorBufferInfo> infos;
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        infos.push_back(m_uniformBuffer.getDescriptorInfo(i));
    return infos;
}

void Renderer::drawScene(const Scene& scene, const Camera& camera)
{
    auto device = m_context.getDevice();
    auto fence = m_inFlightFences[m_currentFrame].get();

    auto wr = device.waitForFences(fence, true, UINT64_MAX);
    (void)wr;

    uint32_t imageIndex;
    try
    {
        auto acquireResult = device.acquireNextImageKHR(
            m_swapchain.getSwapChain(), UINT64_MAX,
            m_imageAvailableSemaphore[m_currentFrame].get(), nullptr);
        imageIndex = acquireResult.value;
    }
    catch(const std::exception& e)
    {
        recreateSwapchain();
        return;
    }
    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        auto wr2 = device.waitForFences(m_imagesInFlight[imageIndex], true, UINT64_MAX);
        (void)wr2;
    }
    m_imagesInFlight[imageIndex] = fence;
    device.resetFences(fence);

    auto cmd = m_commandManager.getCommandBuffer(m_currentFrame);
    cmd.reset();

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


    LOG("shadow indices: y=" << m_pendingLights.count.y << " z=" << m_pendingLights.count.z 
        << " w=" << m_pendingLights.count.w << " numActive=" << m_numActivePointShadows
        << " camPos=(" << camera.getPosition().x << "," << camera.getPosition().y << "," << camera.getPosition().z << ")");

    glm::vec2 jitter = (m_aaMethod == AAMethod::TAA) ? getJitter() : glm::vec2(0.0f);
    glm::mat4 baseProj = camera.getProjectionMatrix(); 

    glm::mat4 proj = baseProj;
    vk::Extent2D extent = m_swapchain.getExtent();
    proj[2][0] += jitter.x / (float)extent.width * 2.0f;
    proj[2][1] += jitter.y / (float)extent.height * 2.0f;

    MVP mvp{};
    mvp.view = camera.getViewMatrix();
    mvp.proj = proj;
    mvp.lightSpaceMatrix = m_lightSpaceMatrix;
    mvp.prevViewProj = m_prevViewProj;
    mvp.viewProjNoJitter = baseProj * camera.getViewMatrix();
    mvp.camPos = glm::vec4(camera.getPosition(), 1.0f);
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

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submitInfo{};
    submitInfo.setWaitSemaphores(m_imageAvailableSemaphore[m_currentFrame].get());
    submitInfo.setWaitDstStageMask(waitStage);
    submitInfo.setCommandBuffers(cmd);
    submitInfo.setSignalSemaphores(m_renderFinishedSemaphore[imageIndex].get());
    m_context.getGraphicQueue().submit(submitInfo, fence);

    auto swapchain = m_swapchain.getSwapChain();
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(m_renderFinishedSemaphore[imageIndex].get());
    presentInfo.setSwapchains(swapchain);
    presentInfo.setImageIndices(imageIndex);

    try
    {
        auto presentResult = m_context.getPresentQueue().presentKHR(presentInfo);
        if (presentResult == vk::Result::eSuboptimalKHR || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapchain();
        }
    }
    catch(const vk::OutOfDateKHRError)
    {
        m_framebufferResized = false;
        recreateSwapchain();
    }
    m_frameIndex++;
    m_historyIndex = 1 - m_historyIndex;
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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

void Renderer::recreateSwapchain()
{
    LOG("recreateSwapchain() called!");
    m_context.getDevice().waitIdle();

    m_context.getDevice().resetDescriptorPool(m_brightPipeline.getDescriptorPool());
    m_context.getDevice().resetDescriptorPool(m_postPipeline.getDescriptorPool());
    m_context.getDevice().resetDescriptorPool(m_fogPipeline.getDescriptorPool());
    m_context.getDevice().resetDescriptorPool(m_godRayMaskPipeline.getDescriptorPool());
    m_context.getDevice().resetDescriptorPool(m_godRayBlurPipeline.getDescriptorPool());
    m_context.getDevice().resetDescriptorPool(m_godRayCompositePipeline.getDescriptorPool());

    m_framebuffers.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.recreate(m_context, m_window);
    vk::Extent2D extent = m_swapchain.getExtent();

    m_godRayMaskFramebuffer.reset();
    m_godRayBlurFramebuffer.reset();
    m_godRayMaskTarget.cleanup();
    m_godRayBlurTarget.cleanup();
    m_godRayMaskTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);
    m_godRayBlurTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    vk::ImageView maskView = m_godRayMaskTarget.getColorView();
    vk::FramebufferCreateInfo maskFbInfo{};
    maskFbInfo.setRenderPass(m_postRenderPass.getRenderPass());
    maskFbInfo.setAttachmentCount(1);
    maskFbInfo.setPAttachments(&maskView);
    maskFbInfo.setWidth(extent.width);
    maskFbInfo.setHeight(extent.height);
    maskFbInfo.setLayers(1);
    m_godRayMaskFramebuffer = m_context.getDevice().createFramebufferUnique(maskFbInfo);

    vk::ImageView blurView = m_godRayBlurTarget.getColorView();
    vk::FramebufferCreateInfo blurFbInfo{};
    blurFbInfo.setRenderPass(m_postRenderPass.getRenderPass());
    blurFbInfo.setAttachmentCount(1);
    blurFbInfo.setPAttachments(&blurView);
    blurFbInfo.setWidth(extent.width);
    blurFbInfo.setHeight(extent.height);
    blurFbInfo.setLayers(1);
    m_godRayBlurFramebuffer = m_context.getDevice().createFramebufferUnique(blurFbInfo);

    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    createFogDescriptorSets();

    m_velocityTarget.cleanup();
    m_velocityTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    m_hdrFramebuffer.reset();
    m_hdrTarget.cleanup();
    m_hdrTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    std::array<vk::ImageView, 3> hdrAttachment = {
        m_hdrTarget.getColorView(),
        m_velocityTarget.getColorView(),
        m_depthBuffer.getImageView()
    };
    vk::FramebufferCreateInfo hdrFbInfo{};
    hdrFbInfo.setRenderPass(m_hdrRenderPass.getRenderPass());
    hdrFbInfo.setAttachments(hdrAttachment);
    hdrFbInfo.setWidth(extent.width);
    hdrFbInfo.setHeight(extent.height);
    hdrFbInfo.setLayers(1);
    m_hdrFramebuffer = m_context.getDevice().createFramebufferUnique(hdrFbInfo);

    m_hdrDecalFramebuffer.reset();
    vk::ImageView hdrDecalView = m_hdrTarget.getColorView();
    vk::FramebufferCreateInfo hdrDecalFbInfo{};
    hdrDecalFbInfo.setRenderPass(m_hdrDecalRenderPass.getRenderPass());
    hdrDecalFbInfo.setAttachmentCount(1);
    hdrDecalFbInfo.setPAttachments(&hdrDecalView);
    hdrDecalFbInfo.setWidth(extent.width);
    hdrDecalFbInfo.setHeight(extent.height);
    hdrDecalFbInfo.setLayers(1);
    m_hdrDecalFramebuffer = m_context.getDevice().createFramebufferUnique(hdrDecalFbInfo);

    m_brightFramebuffer.reset();
    m_brighTarget.cleanup();
    m_brighTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    vk::ImageView brightView = m_brighTarget.getColorView();
    vk::FramebufferCreateInfo brightFbInfo{};
    brightFbInfo.setRenderPass(m_postRenderPass.getRenderPass());
    brightFbInfo.setAttachmentCount(1);
    brightFbInfo.setPAttachments(&brightView);
    brightFbInfo.setWidth(extent.width);
    brightFbInfo.setHeight(extent.height);
    brightFbInfo.setLayers(1);
    m_brightFramebuffer = m_context.getDevice().createFramebufferUnique(brightFbInfo);

    for (auto& fb : m_bloomFramebuffers) fb.reset();
    m_bloomChain.cleanup();
    m_bloomChain.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        vk::ImageView view = m_bloomChain.getMipView(level);
        vk::FramebufferCreateInfo fbInfo{};
        fbInfo.setRenderPass(m_postRenderPass.getRenderPass());
        fbInfo.setAttachmentCount(1);
        fbInfo.setPAttachments(&view);
        vk::Extent2D mipExtent = m_bloomChain.getMipExtent(level);
        fbInfo.setWidth(mipExtent.width);
        fbInfo.setHeight(mipExtent.height);
        fbInfo.setLayers(1);
        m_bloomFramebuffers[level] = m_context.getDevice().createFramebufferUnique(fbInfo);
    }

    m_ldrFramebuffer.reset();
    m_ldrTarget.cleanup();
    m_ldrTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    vk::ImageView ldrView = m_ldrTarget.getColorView();
    vk::FramebufferCreateInfo ldrFbInfo{};
    ldrFbInfo.setRenderPass(m_ldrRenderPass.getRenderPass());
    ldrFbInfo.setAttachmentCount(1);
    ldrFbInfo.setPAttachments(&ldrView);
    ldrFbInfo.setWidth(extent.width);
    ldrFbInfo.setHeight(extent.height);
    ldrFbInfo.setLayers(1);
    m_ldrFramebuffer = m_context.getDevice().createFramebufferUnique(ldrFbInfo);

    for (auto& fb : m_taaFramebuffers) fb.reset();
    m_historyBuffer.cleanup();
    m_historyBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);

    for (uint32_t i = 0; i < 2; i++)
    {
        vk::ImageView view = m_historyBuffer.getView(i);
        vk::FramebufferCreateInfo fbInfo{};
        fbInfo.setRenderPass(m_postRenderPass.getRenderPass());
        fbInfo.setAttachmentCount(1);
        fbInfo.setPAttachments(&view);
        fbInfo.setWidth(extent.width);
        fbInfo.setHeight(extent.height);
        fbInfo.setLayers(1);
        m_taaFramebuffers[i] = m_context.getDevice().createFramebufferUnique(fbInfo);
    }

    auto writeDescSet = [&](vk::DescriptorSet set, vk::ImageView view, vk::Sampler sampler) {
        vk::DescriptorImageInfo info{};
        info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        info.setImageView(view);
        info.setSampler(sampler);

        vk::WriteDescriptorSet write{};
        write.setDstSet(set);
        write.setDstBinding(0);
        write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        write.setDescriptorCount(1);
        write.setImageInfo(info);
        m_context.getDevice().updateDescriptorSets(write, nullptr);
    };

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        vk::ImageView srcView = (level == 0) ? m_brighTarget.getColorView() : m_bloomChain.getMipView(level - 1);
        vk::Sampler srcSampler = (level == 0) ? m_brighTarget.getSampler() : m_bloomChain.getSampler();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_downsampleDescriptorSets[level][i], srcView, srcSampler);
    }

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT - 1; level++)
    {
        vk::ImageView srcView = m_bloomChain.getMipView(level + 1);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            writeDescSet(m_upsampleDescriptorSets[level][i], srcView, m_bloomChain.getSampler());
    }

    std::vector<vk::DescriptorSetLayout> godRayMaskLayouts(MAX_FRAMES_IN_FLIGHT, m_godRayMaskPipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo godRayMaskAllocInfo{};
    godRayMaskAllocInfo.setDescriptorPool(m_godRayMaskPipeline.getDescriptorPool());
    godRayMaskAllocInfo.setSetLayouts(godRayMaskLayouts);
    m_godRayMaskDescriptorSets = m_context.getDevice().allocateDescriptorSets(godRayMaskAllocInfo);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayMaskDescriptorSets[i], m_depthBuffer.getImageView(), m_depthBuffer.getSampler());
    
    std::vector<vk::DescriptorSetLayout> godRayBlurLayouts(MAX_FRAMES_IN_FLIGHT, m_godRayBlurPipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo godRayBlurAllocInfo{};
    godRayBlurAllocInfo.setDescriptorPool(m_godRayBlurPipeline.getDescriptorPool());
    godRayBlurAllocInfo.setSetLayouts(godRayBlurLayouts);
    m_godRayBlurDescriptorSets = m_context.getDevice().allocateDescriptorSets(godRayBlurAllocInfo);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayBlurDescriptorSets[i], m_godRayMaskTarget.getColorView(), m_godRayMaskTarget.getSampler());
    
    std::vector<vk::DescriptorSetLayout> godRayCompositeLayouts(MAX_FRAMES_IN_FLIGHT, m_godRayCompositePipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo godRayCompositeAllocInfo{};
    godRayCompositeAllocInfo.setDescriptorPool(m_godRayCompositePipeline.getDescriptorPool());
    godRayCompositeAllocInfo.setSetLayouts(godRayCompositeLayouts);
    m_godRayCompositeDescriptorSets = m_context.getDevice().allocateDescriptorSets(godRayCompositeAllocInfo);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_godRayCompositeDescriptorSets[i], m_godRayBlurTarget.getColorView(), m_godRayBlurTarget.getSampler());

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        uint32_t prevSlot = 1 - slot;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            std::array<vk::DescriptorImageInfo, 3> infos{};
            infos[0].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            infos[0].setImageView(m_hdrTarget.getColorView());
            infos[0].setSampler(m_hdrTarget.getSampler());

            infos[1].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            infos[1].setImageView(m_velocityTarget.getColorView());
            infos[1].setSampler(m_velocityTarget.getSampler());

            infos[2].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            infos[2].setImageView(m_historyBuffer.getView(prevSlot));
            infos[2].setSampler(m_historyBuffer.getSampler());

            std::array<vk::WriteDescriptorSet, 3> writes{};
            for (uint32_t b = 0; b < 3; b++)
            {
                writes[b].setDstSet(m_taaDescriptorSets[slot][i]);
                writes[b].setDstBinding(b);
                writes[b].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                writes[b].setDescriptorCount(1);
                writes[b].setImageInfo(infos[b]);
            }
            m_context.getDevice().updateDescriptorSets(writes, nullptr);
        }
    }

    {
        auto allocBrightDescSets = [&]() {
            std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_brightPipeline.getDescriptorSetLayout());
            vk::DescriptorSetAllocateInfo allocInfo{};
            allocInfo.setDescriptorPool(m_brightPipeline.getDescriptorPool());
            allocInfo.setSetLayouts(layouts);
            return m_context.getDevice().allocateDescriptorSets(allocInfo);
        };

        m_brightDescriptorSetsRaw = allocBrightDescSets();
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorImageInfo info{};
            info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            info.setImageView(m_hdrTarget.getColorView());
            info.setSampler(m_hdrTarget.getSampler());

            vk::WriteDescriptorSet write{};
            write.setDstSet(m_brightDescriptorSetsRaw[i]);
            write.setDstBinding(0);
            write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            write.setDescriptorCount(1);
            write.setImageInfo(info);
            m_context.getDevice().updateDescriptorSets(write, nullptr);
        }

        m_brightDescriptorSets[0] = allocBrightDescSets();
        m_brightDescriptorSets[1] = allocBrightDescSets();
        for (uint32_t slot = 0; slot < 2; slot++)
        {
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vk::DescriptorImageInfo info{};
                info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                info.setImageView(m_historyBuffer.getView(slot));
                info.setSampler(m_historyBuffer.getSampler());

                vk::WriteDescriptorSet write{};
                write.setDstSet(m_brightDescriptorSets[slot][i]);
                write.setDstBinding(0);
                write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                write.setDescriptorCount(1);
                write.setImageInfo(info);
                m_context.getDevice().updateDescriptorSets(write, nullptr);
            }
        }
    }

    for (uint32_t slot = 0; slot < 2; slot++)
    {
        std::vector<vk::DescriptorSetLayout> postLayouts(MAX_FRAMES_IN_FLIGHT, m_postPipeline.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo postAlloc{};
        postAlloc.setDescriptorPool(m_postPipeline.getDescriptorPool());
        postAlloc.setSetLayouts(postLayouts);
        m_postDescriptorSets[slot] = m_context.getDevice().allocateDescriptorSets(postAlloc);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorImageInfo hdrInfo{};
            hdrInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            hdrInfo.setImageView(m_historyBuffer.getView(slot));
            hdrInfo.setSampler(m_historyBuffer.getSampler());

            vk::DescriptorImageInfo bloomInfo{};
            bloomInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            bloomInfo.setImageView(m_bloomChain.getMipView(0));
            bloomInfo.setSampler(m_bloomChain.getSampler());

            std::array<vk::WriteDescriptorSet, 2> writes{};
            writes[0].setDstSet(m_postDescriptorSets[slot][i]);
            writes[0].setDstBinding(0);
            writes[0].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[0].setDescriptorCount(1);
            writes[0].setImageInfo(hdrInfo);

            writes[1].setDstSet(m_postDescriptorSets[slot][i]);
            writes[1].setDstBinding(1);
            writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[1].setDescriptorCount(1);
            writes[1].setImageInfo(bloomInfo);

            m_context.getDevice().updateDescriptorSets(writes, nullptr);
        }
    }

    {
        std::vector<vk::DescriptorSetLayout> postLayoutsRaw(MAX_FRAMES_IN_FLIGHT, m_postPipeline.getDescriptorSetLayout());
        vk::DescriptorSetAllocateInfo postAllocRaw{};
        postAllocRaw.setDescriptorPool(m_postPipeline.getDescriptorPool());
        postAllocRaw.setSetLayouts(postLayoutsRaw);
        m_postDescriptorSetsRaw = m_context.getDevice().allocateDescriptorSets(postAllocRaw);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorImageInfo hdrInfo{};
            hdrInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            hdrInfo.setImageView(m_hdrTarget.getColorView());
            hdrInfo.setSampler(m_hdrTarget.getSampler());

            vk::DescriptorImageInfo bloomInfo{};
            bloomInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            bloomInfo.setImageView(m_bloomChain.getMipView(0));
            bloomInfo.setSampler(m_bloomChain.getSampler());

            std::array<vk::WriteDescriptorSet, 2> writes{};
            writes[0].setDstSet(m_postDescriptorSetsRaw[i]);
            writes[0].setDstBinding(0);
            writes[0].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[0].setDescriptorCount(1);
            writes[0].setImageInfo(hdrInfo);

            writes[1].setDstSet(m_postDescriptorSetsRaw[i]);
            writes[1].setDstBinding(1);
            writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            writes[1].setDescriptorCount(1);
            writes[1].setImageInfo(bloomInfo);

            m_context.getDevice().updateDescriptorSets(writes, nullptr);
        }
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        m_fxaa.updateDescriptorSet(m_context.getDevice(), i, m_ldrTarget.getColorView(), m_ldrTarget.getSampler());

    size_t imageCount = m_swapchain.getImageViews().size();
    if (m_renderFinishedSemaphore.size() != imageCount)
    {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        m_renderFinishedSemaphore.resize(imageCount);
        for (size_t i = 0; i < imageCount; i++)
            m_renderFinishedSemaphore[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
        m_imagesInFlight.assign(imageCount, VK_NULL_HANDLE);
    }
}

void Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex,
    const Scene& scene, const Camera& camera)
{
    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    {
        vk::ClearValue shadowClear{};
        shadowClear.setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo shadowPass{};
        shadowPass.setRenderPass(m_shadowRenderPass.getRenderPass());
        shadowPass.setFramebuffer(m_shadowFramebuffer.get());
        shadowPass.setRenderArea(vk::Rect2D{ {0, 0}, { m_shadowMap.getSize(), m_shadowMap.getSize() } });
        shadowPass.setClearValues(shadowClear);

        commandBuffer.beginRenderPass(shadowPass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_shadowPipeline.getPipeline());

        vk::Viewport vp{};
        vp.setWidth((float)m_shadowMap.getSize()).setHeight((float)m_shadowMap.getSize())
          .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, vp);
        vk::Rect2D sc{};
        sc.setExtent({ m_shadowMap.getSize(), m_shadowMap.getSize() });
        commandBuffer.setScissor(0, sc);

        for (const auto& obj : scene.getObjects())
        {
            if (!obj->mesh) continue;
            obj->mesh->bind(commandBuffer);

            struct ShadowPush { glm::mat4 lightSpace; glm::mat4 model; } spush;
            spush.lightSpace = m_lightSpaceMatrix;
            spush.model = obj->transform.getMatrix();
            commandBuffer.pushConstants(m_shadowPipeline.getLayout(),
                vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShadowPush), &spush);

            for (const auto& sm : obj->mesh->getSubmeshes())
                obj->mesh->drawSubmesh(commandBuffer, sm);
        }
        commandBuffer.endRenderPass();
    }

    LOG("hasPointLightShadow=" << m_hasPointLightShadow << " numActive=" << m_numActivePointShadows);

    if (m_hasPointLightShadow)
    {
        vk::ClearValue shadowClear{};
        shadowClear.setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo atlasPass{};
        atlasPass.setRenderPass(m_pointShadowRenderPass.getRenderPass());
        atlasPass.setFramebuffer(m_shadowAtlasFramebuffer.get());
        atlasPass.setRenderArea(vk::Rect2D{ {0, 0}, { m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() } });
        atlasPass.setClearValues(shadowClear);

        commandBuffer.beginRenderPass(atlasPass, vk::SubpassContents::eInline);   // ОДИН раз, не 18
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pointShadowPipeline.getPipeline());

        for (int slot = 0; slot < m_numActivePointShadows; slot++)
        {
            for (uint32_t face = 0; face < 6; face++)
            {
                vk::Rect2D tile = m_shadowAtlas.getTileRect(slot, face);

                vk::Viewport vp{};
                vp.setX((float)tile.offset.x).setY((float)tile.offset.y);
                vp.setWidth((float)tile.extent.width).setHeight((float)tile.extent.height);
                vp.setMinDepth(0.0f).setMaxDepth(1.0f);
                commandBuffer.setViewport(0, vp);
                commandBuffer.setScissor(0, tile);

                int drawnCount = 0;

                auto facePlanes = extractFrustumPlanes(m_pointLightViewProjArr[slot][face]);

                for (const auto& obj : scene.getObjects())
                {
                    if (!obj->mesh) continue;
                    obj->mesh->bind(commandBuffer);
                
                    glm::mat4 modelMatrix = obj->transform.getMatrix();
                
                    PointShadowPush pspush{};
                    pspush.lightViewProj = m_pointLightViewProjArr[slot][face];
                    pspush.model = modelMatrix;
                    pspush.lightPos = glm::vec4(m_activePointLightPos[slot], m_pointShadowFarPlane);
                
                    commandBuffer.pushConstants(m_pointShadowPipeline.getLayout(),
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                        0, sizeof(PointShadowPush), &pspush);
                
                    for (const auto& sm : obj->mesh->getSubmeshes())
                    {
                        if (!isSubmeshVisible(modelMatrix, sm, facePlanes))
                            continue;
                    
                        obj->mesh->drawSubmesh(commandBuffer, sm);
                    }
                }

                if (slot == 0 && face == 0)
                    LOG("atlas slot0 face0: drawn=" << drawnCount << " tile=(" << tile.offset.x << "," << tile.offset.y 
                        << " " << tile.extent.width << "x" << tile.extent.height << ")");
                
            }
        }

        commandBuffer.endRenderPass();
    }
    else
    {
        vk::ClearValue shadowClear{};
        shadowClear.setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo atlasPass{};
        atlasPass.setRenderPass(m_pointShadowRenderPass.getRenderPass());
        atlasPass.setFramebuffer(m_shadowAtlasFramebuffer.get());
        atlasPass.setRenderArea(vk::Rect2D{ {0, 0}, {m_shadowAtlas.getWidth(), m_shadowAtlas.getHeight() } });
        atlasPass.setClearValues(shadowClear);

        commandBuffer.beginRenderPass(atlasPass, vk::SubpassContents::eInline);
        commandBuffer.endRenderPass();
    }

    {
        std::array<vk::ClearValue, 3> clearValues{};
        clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
        clearValues[1].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
        clearValues[2].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo hdrPass{};
        hdrPass.setRenderPass(m_hdrRenderPass.getRenderPass());
        hdrPass.setFramebuffer(m_hdrFramebuffer.get());
        hdrPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        hdrPass.setClearValues(clearValues);

        commandBuffer.beginRenderPass(hdrPass, vk::SubpassContents::eInline);

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
                .setHeight((float)m_swapchain.getExtent().height)
                .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());

        auto frustumPlanes = extractFrustumPlanes(camera.getProjectionMatrix() * camera.getViewMatrix());

        int visibleCount = 0;
        int totalCount = 0; 
        
        for (const auto& obj : scene.getObjects())
        {
            if (!obj->mesh) continue;
            obj->mesh->bind(commandBuffer);

            glm::mat4 modelMatrix = obj->transform.getMatrix();

            PushConstant pc{};
            pc.model = obj->transform.getMatrix();
            pc.normalMatrix = obj->transform.getNormalMatrix();
            pc.prevModel = obj->transform.getMatrix();
            commandBuffer.pushConstants(m_pipeline.getLayout(),
                vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstant), &pc);

            for (const auto& sm : obj->mesh->getSubmeshes())
            {
                totalCount++;

                if (!isSubmeshVisible(modelMatrix, sm, frustumPlanes))
                    continue;

                visibleCount++;

                auto mat = obj->getMaterial(sm.materialIndex);
                if (!mat) continue;
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    m_pipeline.getLayout(), 0, mat->getDescriptorSet(m_currentFrame), nullptr);
                obj->mesh->drawSubmesh(commandBuffer, sm);
            }
        }

        LOG("visible submeshes: " << visibleCount << " / " << totalCount);

        commandBuffer.endRenderPass();
    }

    if (!m_screenSpaceDecals.empty())
    {
        vk::RenderPassBeginInfo decalPass{};
        decalPass.setRenderPass(m_hdrDecalRenderPass.getRenderPass());
        decalPass.setFramebuffer(m_hdrDecalFramebuffer.get());
        decalPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        decalPass.setClearValueCount(0);

        commandBuffer.beginRenderPass(decalPass, vk::SubpassContents::eInline);

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
                .setHeight((float)m_swapchain.getExtent().height)
                .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_decalPipeline.getPipeline());
        m_decalBox.bind(commandBuffer);

        for (const auto& decal : m_screenSpaceDecals)
        {
            struct DecalPush { glm::mat4 model; glm::mat4 invModel; glm::vec4 params; } dpush;
            dpush.model = decal->getModelMatrix();
            dpush.invModel = decal->getInvModelMatrix();
            dpush.params = glm::vec4(decal->metallic, decal->roughness, 0.0f, 0.0f);
            commandBuffer.pushConstants(m_decalPipeline.getLayout(),
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, sizeof(DecalPush), &dpush);

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                m_decalPipeline.getLayout(), 0, decal->getDescriptorSet(m_currentFrame), nullptr);

            m_decalBox.draw(commandBuffer);
        }

        commandBuffer.endRenderPass();
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

        vk::RenderPassBeginInfo fogPass{};
        fogPass.setRenderPass(m_hdrDecalRenderPass.getRenderPass());
        fogPass.setFramebuffer(m_hdrDecalFramebuffer.get());
        fogPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        fogPass.setClearValueCount(0);

        commandBuffer.beginRenderPass(fogPass, vk::SubpassContents::eInline);

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
                .setHeight((float)m_swapchain.getExtent().height)
                .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_fogPipeline.getPipeline());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_fogPipeline.getLayout(), 0, m_fogDescriptorSets[m_currentFrame], nullptr);

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

        commandBuffer.pushConstants(m_fogPipeline.getLayout(), vk::ShaderStageFlagBits::eFragment,
            0, sizeof(FogPush), &fogPush);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
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
            vk::Extent2D grExtent = m_swapchain.getExtent();

            {
                vk::ClearValue clear{};
                clear.setColor(vk::ClearColorValue{ std::array<float,4>{0,0,0,0} });

                vk::RenderPassBeginInfo pass{};
                pass.setRenderPass(m_postRenderPass.getRenderPass());
                pass.setFramebuffer(m_godRayMaskFramebuffer.get());
                pass.setRenderArea(vk::Rect2D{ {0,0}, grExtent });
                pass.setClearValues(clear);

                commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_godRayMaskPipeline.getPipeline());

                vk::Viewport vp{};
                vp.setWidth((float)grExtent.width).setHeight((float)grExtent.height).setMaxDepth(1.0f);
                commandBuffer.setViewport(0, vp);
                vk::Rect2D sc{}; sc.setExtent(grExtent);
                commandBuffer.setScissor(0, sc);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    m_godRayMaskPipeline.getLayout(), 0, m_godRayMaskDescriptorSets[m_currentFrame], nullptr);

                struct MaskPush { glm::mat4 invViewProj; glm::vec4 sunDirIntensity; glm::vec4 sunColorSize; } maskPush{};
                maskPush.invViewProj = glm::inverse(camera.getProjectionMatrix() * camera.getViewMatrix());
                maskPush.sunDirIntensity = glm::vec4(sunDir, 1.0f);
                maskPush.sunColorSize = glm::vec4(sunColor, m_godRaySettings.sunSize);

                commandBuffer.pushConstants(m_godRayMaskPipeline.getLayout(), vk::ShaderStageFlagBits::eFragment,
                    0, sizeof(MaskPush), &maskPush);

                commandBuffer.draw(3, 1, 0, 0);
                commandBuffer.endRenderPass();
            }

            {
                vk::ClearValue clear{};
                clear.setColor(vk::ClearColorValue{ std::array<float,4>{0,0,0,0} });

                vk::RenderPassBeginInfo pass{};
                pass.setRenderPass(m_postRenderPass.getRenderPass());
                pass.setFramebuffer(m_godRayBlurFramebuffer.get());
                pass.setRenderArea(vk::Rect2D{ {0,0}, grExtent });
                pass.setClearValues(clear);

                commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_godRayBlurPipeline.getPipeline());

                vk::Viewport vp{};
                vp.setWidth((float)grExtent.width).setHeight((float)grExtent.height).setMaxDepth(1.0f);
                commandBuffer.setViewport(0, vp);
                vk::Rect2D sc{}; sc.setExtent(grExtent);
                commandBuffer.setScissor(0, sc);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    m_godRayBlurPipeline.getLayout(), 0, m_godRayBlurDescriptorSets[m_currentFrame], nullptr);

                struct BlurPush { glm::vec4 sunUVParams; glm::vec4 weightCountParams; } blurPush{};
                blurPush.sunUVParams = glm::vec4(sunUV, m_godRaySettings.density, m_godRaySettings.decay);
                blurPush.weightCountParams = glm::vec4(m_godRaySettings.weight, (float)m_godRaySettings.sampleCount, 0.0f, 0.0f);

                commandBuffer.pushConstants(m_godRayBlurPipeline.getLayout(), vk::ShaderStageFlagBits::eFragment,
                    0, sizeof(BlurPush), &blurPush);

                commandBuffer.draw(3, 1, 0, 0);
                commandBuffer.endRenderPass();
            }

            {
                vk::RenderPassBeginInfo pass{};
                pass.setRenderPass(m_hdrDecalRenderPass.getRenderPass());
                pass.setFramebuffer(m_hdrDecalFramebuffer.get());
                pass.setRenderArea(vk::Rect2D{ {0,0}, m_swapchain.getExtent() });
                pass.setClearValueCount(0);

                commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_godRayCompositePipeline.getPipeline());

                vk::Viewport vp{};
                vp.setWidth((float)m_swapchain.getExtent().width).setHeight((float)m_swapchain.getExtent().height).setMaxDepth(1.0f);
                commandBuffer.setViewport(0, vp);
                vk::Rect2D sc{}; sc.setExtent(m_swapchain.getExtent());
                commandBuffer.setScissor(0, sc);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    m_godRayCompositePipeline.getLayout(), 0, m_godRayCompositeDescriptorSets[m_currentFrame], nullptr);

                float intensity = m_godRaySettings.intensity;
                commandBuffer.pushConstants(m_godRayCompositePipeline.getLayout(), vk::ShaderStageFlagBits::eFragment,
                    0, sizeof(float), &intensity);

                commandBuffer.draw(3, 1, 0, 0);
                commandBuffer.endRenderPass();
            }
        }
    }

    if (m_aaMethod == AAMethod::TAA)
    {
        uint32_t writeSlot = m_historyIndex;

        vk::ClearValue clear{};
        clear.setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });

        vk::RenderPassBeginInfo pass{};
        pass.setRenderPass(m_postRenderPass.getRenderPass());
        pass.setFramebuffer(m_taaFramebuffers[writeSlot].get());
        pass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        pass.setClearValues(clear);

        commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_taaPipeline.getPipeline());

        vk::Viewport vp{};
        vp.setWidth((float)m_swapchain.getExtent().width).setHeight((float)m_swapchain.getExtent().height)
          .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, vp);
        vk::Rect2D sc{};
        sc.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, sc);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_taaPipeline.getLayout(), 0, m_taaDescriptorSets[writeSlot][m_currentFrame], nullptr);

        commandBuffer.pushConstants(m_taaPipeline.getLayout(),
            vk::ShaderStageFlagBits::eFragment, 0, sizeof(float), &m_taaBlendFactor);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    {
        vk::ClearValue clear{};
        clear.setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });

        vk::RenderPassBeginInfo pass{};
        pass.setRenderPass(m_postRenderPass.getRenderPass());
        pass.setFramebuffer(m_brightFramebuffer.get());
        pass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        pass.setClearValues(clear);

        commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_brightPipeline.getPipeline());

        vk::Viewport vp{};
        vp.setWidth((float)m_swapchain.getExtent().width).setHeight((float)m_swapchain.getExtent().height)
          .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, vp);
        vk::Rect2D sc{};
        sc.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, sc);

        vk::DescriptorSet brightSet = (m_aaMethod == AAMethod::TAA)
            ? m_brightDescriptorSets[m_historyIndex][m_currentFrame]
            : m_brightDescriptorSetsRaw[m_currentFrame];

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_brightPipeline.getLayout(), 0, brightSet, nullptr);

        commandBuffer.pushConstants(m_brightPipeline.getLayout(),
            vk::ShaderStageFlagBits::eFragment, 0, sizeof(float), &m_bloomThreshold);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    for (uint32_t level = 0; level < BloomChain::MIP_COUNT; level++)
    {
        vk::ClearValue clear{};
        clear.setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });

        vk::RenderPassBeginInfo pass{};
        pass.setRenderPass(m_postRenderPass.getRenderPass());
        pass.setFramebuffer(m_bloomFramebuffers[level].get());
        vk::Extent2D mipExtent = m_bloomChain.getMipExtent(level);
        pass.setRenderArea(vk::Rect2D{ {0, 0}, mipExtent });
        pass.setClearValues(clear);

        commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_downsamplePipeline.getPipeline());

        vk::Viewport vp{};
        vp.setWidth((float)mipExtent.width).setHeight((float)mipExtent.height)
          .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, vp);
        vk::Rect2D sc{};
        sc.setExtent(mipExtent);
        commandBuffer.setScissor(0, sc);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_downsamplePipeline.getLayout(), 0, m_downsampleDescriptorSets[level][m_currentFrame], nullptr);

        vk::Extent2D srcExtent = (level == 0) ? m_swapchain.getExtent() : m_bloomChain.getMipExtent(level - 1);
        glm::vec2 srcTexelSize(1.0f / srcExtent.width, 1.0f / srcExtent.height);
        commandBuffer.pushConstants(m_downsamplePipeline.getLayout(),
            vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::vec2), &srcTexelSize);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    for (int level = (int)BloomChain::MIP_COUNT - 2; level >= 0; level--)
    {
        vk::RenderPassBeginInfo pass{};
        pass.setRenderPass(m_postRenderPassLoad.getRenderPass());
        pass.setFramebuffer(m_bloomFramebuffers[level].get());
        vk::Extent2D mipExtent = m_bloomChain.getMipExtent(level);
        pass.setRenderArea(vk::Rect2D{ {0, 0}, mipExtent });
        pass.setClearValueCount(0);

        commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_upsamplePipeline.getPipeline());

        vk::Viewport vp{};
        vp.setWidth((float)mipExtent.width).setHeight((float)mipExtent.height)
          .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, vp);
        vk::Rect2D sc{};
        sc.setExtent(mipExtent);
        commandBuffer.setScissor(0, sc);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_upsamplePipeline.getLayout(), 0, m_upsampleDescriptorSets[level][m_currentFrame], nullptr);

        float radius = 1.0f;
        commandBuffer.pushConstants(m_upsamplePipeline.getLayout(),
            vk::ShaderStageFlagBits::eFragment, 0, sizeof(float), &radius);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    {
        vk::ClearValue clear{};
        clear.setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });

        vk::RenderPassBeginInfo pass{};
        pass.setRenderPass(m_ldrRenderPass.getRenderPass());
        pass.setFramebuffer(m_ldrFramebuffer.get());
        pass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        pass.setClearValues(clear);

        commandBuffer.beginRenderPass(pass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_postPipeline.getPipeline());

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
                .setHeight((float)m_swapchain.getExtent().height)
                .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        vk::DescriptorSet postSet = (m_aaMethod == AAMethod::TAA)
            ? m_postDescriptorSets[m_historyIndex][m_currentFrame]
            : m_postDescriptorSetsRaw[m_currentFrame];

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_postPipeline.getLayout(), 0, postSet, nullptr);

        struct TonemapPush { float exposure; float contrast; } tmPush{ m_exposure, m_contrast };
        commandBuffer.pushConstants(m_postPipeline.getLayout(),
            vk::ShaderStageFlagBits::eFragment, 0, sizeof(TonemapPush), &tmPush);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    {
        std::array<vk::ClearValue, 2> clearValues{};
        clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
        clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo finalPass{};
        finalPass.setRenderPass(m_renderPass.getRenderPass());
        finalPass.setFramebuffer(m_framebuffers.getFramebuffers()[imageIndex].get());
        finalPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        finalPass.setClearValues(clearValues);

        commandBuffer.beginRenderPass(finalPass, vk::SubpassContents::eInline);

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
            .setHeight((float)m_swapchain.getExtent().height)
            .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        m_fxaa.apply(commandBuffer, m_currentFrame, m_ldrTarget.getColorView(), m_ldrTarget.getSampler());

        renderImGui(commandBuffer);

        commandBuffer.endRenderPass();
    }

    commandBuffer.end();
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
            m_antiAliasing->initialize(m_context.getDevice(), m_context.getPhysicalDevice(),
                m_swapchain.getExtent(), m_pipelineCache.get());
            break;
    }
}

void Renderer::cleanup()
{
    if (m_cleaned) return;
    m_cleaned = true;

    cleanupImGui();
    m_imguiDescriptorPool.reset();

    m_context.getDevice().waitIdle();

    m_godRayMaskPipeline.cleanup();
    m_godRayBlurPipeline.cleanup();
    m_godRayCompositePipeline.cleanup();
    m_godRayMaskTarget.cleanup();
    m_godRayBlurTarget.cleanup();

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

    m_pipelineCache.save();
    m_pipelineCache.cleanup();
    m_fogPipeline.cleanup();
    m_fogCameraBuffer.cleanup();

    m_hdrFramebuffer.reset();
    m_postPipeline.cleanup();
    m_hdrRenderPass.cleanup();
    m_hdrTarget.cleanup();

    m_velocityTarget.cleanup();

    m_decalBox.cleanup();
    m_decalCameraBuffer.cleanup();
    m_decalPipeline.cleanup();
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
    m_brighTarget.cleanup();

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
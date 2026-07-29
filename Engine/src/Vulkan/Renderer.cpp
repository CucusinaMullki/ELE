#include "Vulkan/Renderer.h"
#include "Vulkan/PushConstant.h"
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

    // ═══ Tonemap pipeline создаётся ЗДЕСЬ ═══
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
    m_postPipeline.initialize(m_context.getDevice(), postConfig);   // ← ПОСЛЕ ЭТОЙ СТРОКИ pipeline существует

    // TAA-based tonemap descriptor sets
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

    // ═══ ПЕРЕМЕЩЕНО СЮДА: Raw tonemap descriptor set (m_postPipeline уже создан) ═══
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
    decalConfig.pushConstantSize = sizeof(glm::mat4) * 2;
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

    m_pointShadowMap.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), 1024);
    m_pointShadowRenderPass.initialize(m_context.getDevice(), m_pointShadowMap.getFormat());

    for (uint32_t face = 0; face < 6; face++)
    {
        vk::ImageView faceView = m_pointShadowMap.getFaceView(face);
        vk::FramebufferCreateInfo fbInfo{};
        fbInfo.setRenderPass(m_pointShadowRenderPass.getRenderPass());
        fbInfo.setAttachmentCount(1);
        fbInfo.setPAttachments(&faceView);
        fbInfo.setWidth(m_pointShadowMap.getSize());
        fbInfo.setHeight(m_pointShadowMap.getSize());
        fbInfo.setLayers(1);
        m_pointShadowFramebuffers[face] = m_context.getDevice().createFramebufferUnique(fbInfo);
    }

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

        cmd->end();

        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(cmd.get());
        m_context.getGraphicQueue().submit(submitInfo, nullptr);
        m_context.getGraphicQueue().waitIdle();
    }
}

void Renderer::updatePointLightMatrices(glm::vec3 lightPos)
{
    float aspect = 1.0f;
    float nearPlane = 0.1f;
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect, nearPlane, m_pointShadowFarPlane);

    m_pointLightViewProj[0] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0));
    m_pointLightViewProj[1] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0));
    m_pointLightViewProj[2] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1));
    m_pointLightViewProj[3] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1));
    m_pointLightViewProj[4] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0));
    m_pointLightViewProj[5] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0));
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

    glm::vec3 pointLightPos(0.0f);
    bool hasPointLight = false;
    for (int i = 0; i < m_pendingLights.count.x; i++)
    {
        if (m_pendingLights.lights[i].position.w >= 0.5f)
        {
            pointLightPos = glm::vec3(m_pendingLights.lights[i].position);
            hasPointLight = true;
            break;
        }
    }
    if (hasPointLight)
        updatePointLightMatrices(pointLightPos);

    m_hasPointLightShadow = hasPointLight;
    m_currentPointLightPos = pointLightPos;

    glm::vec2 jitter = getJitter();
    glm::mat4 baseProj = camera.getProjectionMatrix(); 

    glm::mat4 proj = baseProj;
    vk::Extent2D extent = m_swapchain.getExtent();
    proj[2][0] += jitter.x / (float)extent.width * 2.0f;
    proj[2][1] += jitter.y / (float)extent.height * 2.0f;

    MVP mvp{};
    mvp.view = camera.getViewMatrix();
    mvp.proj = proj;
    mvp.lightSpaceMatrix = m_lightSpaceMatrix;
    mvp.camPos = glm::vec4(camera.getPosition(), 1.0f);
    m_uniformBuffer.update(m_currentFrame, mvp);
    m_lightBuffer.update(m_currentFrame, m_pendingLights);

    DecalCameraData decalCamData{};
    decalCamData.view = camera.getViewMatrix();
    decalCamData.proj = baseProj;
    decalCamData.invViewProj = glm::inverse(decalCamData.proj * decalCamData.view);
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

void Renderer::recreateSwapchain()
{
    LOG("recreateSwapchain() called!");
    m_context.getDevice().waitIdle();

    m_context.getDevice().resetDescriptorPool(m_brightPipeline.getDescriptorPool());
    m_context.getDevice().resetDescriptorPool(m_postPipeline.getDescriptorPool());

    m_framebuffers.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.recreate(m_context, m_window);
    vk::Extent2D extent = m_swapchain.getExtent();

    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), extent);
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

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

    if (m_hasPointLightShadow)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            vk::ClearValue shadowClear{};
            shadowClear.setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

            vk::RenderPassBeginInfo facePass{};
            facePass.setRenderPass(m_pointShadowRenderPass.getRenderPass());
            facePass.setFramebuffer(m_pointShadowFramebuffers[face].get());
            facePass.setRenderArea(vk::Rect2D{ {0, 0}, { m_pointShadowMap.getSize(), m_pointShadowMap.getSize() } });
            facePass.setClearValues(shadowClear);

            commandBuffer.beginRenderPass(facePass, vk::SubpassContents::eInline);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pointShadowPipeline.getPipeline());

            vk::Viewport vp{};
            vp.setWidth((float)m_pointShadowMap.getSize()).setHeight((float)m_pointShadowMap.getSize())
                .setMinDepth(0.0f).setMaxDepth(1.0f);
            commandBuffer.setViewport(0, vp);
            vk::Rect2D sc{};
            sc.setExtent({ m_pointShadowMap.getSize(), m_pointShadowMap.getSize() });
            commandBuffer.setScissor(0, sc);

            for (const auto& obj : scene.getObjects())
            {
                if (!obj->mesh) continue;
                obj->mesh->bind(commandBuffer);

                PointShadowPush psPush{};
                psPush.lightViewProj = m_pointLightViewProj[face];
                psPush.model = obj->transform.getMatrix();
                psPush.lightPos = glm::vec4(m_currentPointLightPos, m_pointShadowFarPlane);

                commandBuffer.pushConstants(m_pointShadowPipeline.getLayout(),
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                    0, sizeof(PointShadowPush), &psPush);

                for (const auto& sm : obj->mesh->getSubmeshes())
                    obj->mesh->drawSubmesh(commandBuffer, sm);
            }
            commandBuffer.endRenderPass();
        }
    }
    else
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            vk::ClearValue shadowClear{};
            shadowClear.setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

            vk::RenderPassBeginInfo facePass{};
            facePass.setRenderPass(m_pointShadowRenderPass.getRenderPass());
            facePass.setFramebuffer(m_pointShadowFramebuffers[face].get());
            facePass.setRenderArea(vk::Rect2D{ {0, 0}, { m_pointShadowMap.getSize(), m_pointShadowMap.getSize() } });
            facePass.setClearValues(shadowClear);

            commandBuffer.beginRenderPass(facePass, vk::SubpassContents::eInline);
            commandBuffer.endRenderPass();
        }
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

        for (const auto& obj : scene.getObjects())
        {
            if (!obj->mesh) continue;
            obj->mesh->bind(commandBuffer);

            PushConstant pc{};
            pc.model = obj->transform.getMatrix();
            pc.normalMatrix = obj->transform.getNormalMatrix();
            pc.prevModel = obj->transform.getMatrix();
            commandBuffer.pushConstants(m_pipeline.getLayout(),
                vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstant), &pc);

            for (const auto& sm : obj->mesh->getSubmeshes())
            {
                auto mat = obj->getMaterial(sm.materialIndex);
                if (!mat) continue;
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    m_pipeline.getLayout(), 0, mat->getDescriptorSet(m_currentFrame), nullptr);
                obj->mesh->drawSubmesh(commandBuffer, sm);
            }
        }

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
            struct DecalPush { glm::mat4 model; glm::mat4 invModel; } dpush;
            dpush.model = decal->getModelMatrix();
            dpush.invModel = decal->getInvModelMatrix();
            commandBuffer.pushConstants(m_decalPipeline.getLayout(),
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, sizeof(DecalPush), &dpush);

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                m_decalPipeline.getLayout(), 0, decal->getDescriptorSet(m_currentFrame), nullptr);

            m_decalBox.draw(commandBuffer);
        }

        commandBuffer.endRenderPass();
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

        commandBuffer.endRenderPass();
    }

    commandBuffer.end();
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

    m_context.getDevice().waitIdle();

    m_decalPipeline.cleanup();
    m_fxaa.cleanup();
    m_ldrFramebuffer.reset();
    m_ldrRenderPass.cleanup();
    m_ldrTarget.cleanup();

    m_pipelineCache.save();
    m_pipelineCache.cleanup();

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

    for (auto& fb : m_pointShadowFramebuffers) fb.reset();
    m_pointShadowPipeline.cleanup();
    m_pointShadowRenderPass.cleanup();
    m_pointShadowMap.cleanup();

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
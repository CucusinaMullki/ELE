#include "Vulkan/Renderer.h"
#include "Vulkan/PushConstant.h"
#include "Core/Log.h"

void Renderer::initialize(GLFWwindow* window)
{
    m_window = window;

    m_context.initialize(m_window);
    m_swapchain.initialize(m_context, m_window);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_renderPass.initialize(m_context.getDevice(), m_swapchain.getImageFormat(), m_depthBuffer.getFormat());
    m_pipelineCache.initialize(m_context.getDevice());

    m_hdrTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_hdrRenderPass.initialize(m_context.getDevice(), m_hdrTarget.getColorFormat(), m_depthBuffer.getFormat());

    std::array<vk::ImageView, 2> hdrAttachment = {
        m_hdrTarget.getColorView(),
        m_depthBuffer.getImageView()
    };
    vk::FramebufferCreateInfo hdrFbInfo{};
    hdrFbInfo.setRenderPass(m_hdrRenderPass.getRenderPass());
    hdrFbInfo.setAttachments(hdrAttachment);
    hdrFbInfo.setWidth(m_swapchain.getExtent().width);
    hdrFbInfo.setHeight(m_swapchain.getExtent().height);
    hdrFbInfo.setLayers(1);
    m_hdrFramebuffer = m_context.getDevice().createFramebufferUnique(hdrFbInfo);

    vk::Extent2D bloomExtent = m_swapchain.getExtent();
    m_brighTarget.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), bloomExtent);
    m_postRenderPass.initialize(m_context.getDevice(), m_brighTarget.getColorFormat());

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

    m_brightDescriptorSets = allocDescSets(m_brightPipeline);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        writeDescSet(m_brightDescriptorSets[i], m_hdrTarget.getColorView(), m_hdrTarget.getSampler());

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

    PipelineConfig postConfig{};
    postConfig.vertexShader = "fullscreen";
    postConfig.fragmentShader = "tonemap";
    postConfig.renderPass = m_renderPass.getRenderPass();
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

    std::vector<vk::DescriptorSetLayout> postLayouts(MAX_FRAMES_IN_FLIGHT, m_postPipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo postAlloc{};
    postAlloc.setDescriptorPool(m_postPipeline.getDescriptorPool());
    postAlloc.setSetLayouts(postLayouts);
    m_postDescriptorSets = m_context.getDevice().allocateDescriptorSets(postAlloc);

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
        writes[0].setDstSet(m_postDescriptorSets[i]);
        writes[0].setDstBinding(0);
        writes[0].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        writes[0].setDescriptorCount(1);
        writes[0].setImageInfo(hdrInfo);

        writes[1].setDstSet(m_postDescriptorSets[i]);
        writes[1].setDstBinding(1);
        writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        writes[1].setDescriptorCount(1);
        writes[1].setImageInfo(bloomInfo);

        m_context.getDevice().updateDescriptorSets(writes, nullptr);
    }

    PipelineConfig pbrConfig{};
    pbrConfig.vertexShader = "pbr";
    pbrConfig.fragmentShader = "pbr";
    pbrConfig.renderPass = m_hdrRenderPass.getRenderPass();
    pbrConfig.cache = m_pipelineCache.get();
    pbrConfig.frameCount = MAX_FRAMES_IN_FLIGHT;
    pbrConfig.useDescriptors = true;
    pbrConfig.colorAttachmentCount = 1;
    pbrConfig.cullMode = vk::CullModeFlagBits::eBack;
    m_pipeline.initialize(m_context.getDevice(), pbrConfig);

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

    MVP mvp{};
    mvp.view = camera.getViewMatrix();
    mvp.proj = camera.getProjectionMatrix();
    mvp.lightSpaceMatrix = m_lightSpaceMatrix;
    mvp.camPos = glm::vec4(camera.getPosition(), 1.0f);
    m_uniformBuffer.update(m_currentFrame, mvp);
    m_lightBuffer.update(m_currentFrame, m_pendingLights);

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

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recreateSwapchain()
{
    m_context.getDevice().waitIdle();
    m_framebuffers.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.recreate(m_context, m_window);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

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
        std::array<vk::ClearValue, 2> clearValues{};
        clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
        clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo hdrPass{};
        hdrPass.setRenderPass(m_hdrRenderPass.getRenderPass());
        hdrPass.setFramebuffer(m_hdrFramebuffer.get());
        hdrPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        hdrPass.setClearValues(clearValues);

        commandBuffer.beginRenderPass(hdrPass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
                .setHeight((float)m_swapchain.getExtent().height)
                .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        for (const auto& obj : scene.getObjects())
        {
            if (!obj->mesh) continue;
            obj->mesh->bind(commandBuffer);

            PushConstant pc{};
            pc.model = obj->transform.getMatrix();
            pc.normalMatrix = obj->transform.getNormalMatrix();
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

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_brightPipeline.getLayout(), 0, m_brightDescriptorSets[m_currentFrame], nullptr);

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
        std::array<vk::ClearValue, 2> clearValues{};
        clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f } });
        clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

        vk::RenderPassBeginInfo postPass{};
        postPass.setRenderPass(m_renderPass.getRenderPass());
        postPass.setFramebuffer(m_framebuffers.getFramebuffers()[imageIndex].get());
        postPass.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
        postPass.setClearValues(clearValues);

        commandBuffer.beginRenderPass(postPass, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_postPipeline.getPipeline());

        vk::Viewport viewport{};
        viewport.setWidth((float)m_swapchain.getExtent().width)
            .setHeight((float)m_swapchain.getExtent().height)
            .setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer.setViewport(0, viewport);
        vk::Rect2D scissor{};
        scissor.setExtent(m_swapchain.getExtent());
        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_postPipeline.getLayout(), 0, m_postDescriptorSets[m_currentFrame], nullptr);

        struct TonemapPush { float exposure; float contrast; } tmPush{ m_exposure, m_contrast };
        commandBuffer.pushConstants(m_postPipeline.getLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(TonemapPush), &tmPush);

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    commandBuffer.end();
}

void Renderer::cleanup()
{
    if (m_cleaned) return;
    m_cleaned = true;

    m_context.getDevice().waitIdle();

    m_pipelineCache.save();
    m_pipelineCache.cleanup();

    m_hdrFramebuffer.reset();
    m_postPipeline.cleanup();
    m_hdrRenderPass.cleanup();
    m_hdrTarget.cleanup();

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
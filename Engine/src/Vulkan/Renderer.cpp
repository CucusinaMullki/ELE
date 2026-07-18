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
    m_pipeline.initialize(m_context.getDevice(), m_renderPass.getRenderPass(), "shader", MAX_FRAMES_IN_FLIGHT);

    auto indices = m_context.GetQueueFamilyIndices();
    m_commandManager.intialize(m_context.getDevice(), indices.graphicFamily.value(), MAX_FRAMES_IN_FLIGHT);

    m_uniformBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    LOG("Swapchain images: " << m_swapchain.getImageViews().size());
    LOG("FrameBuffers: " << m_framebuffers.getFramebuffers().size());

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
        m_imageAvailableSemaphore[i]= m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
        m_inFlightFences[i] = m_context.getDevice().createFenceUnique(fenceInfo);
    }
    for (size_t i = 0; i < imageCount; i++)
        m_renderFinishedSemaphore[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
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

    MVP mvp{};
    mvp.view = camera.getViewMatrix();
    mvp.proj = camera.getProjectionMatrix();
    mvp.camPos = glm::vec4(camera.getPosition(), 1.0f);
    mvp.lightPos = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    mvp.lightColor = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) * 3.0f;
    m_uniformBuffer.update(m_currentFrame, mvp);

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

void Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex,
    const Scene& scene, const Camera& camera)
{
    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    std::array<vk::ClearValue, 2> clearValues{};
    
    clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.02f, 0.02f, 0.03f, 1.0f } });
    clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.setRenderPass(m_renderPass.getRenderPass());
    renderPassInfo.setFramebuffer(m_framebuffers.getFramebuffers()[imageIndex].get());
    renderPassInfo.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
    renderPassInfo.setClearValues(clearValues);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());

    vk::Viewport viewport{};
    viewport.setX(0.0f).setY(0.0f);
    viewport.setWidth((float)m_swapchain.getExtent().width);
    viewport.setHeight((float)m_swapchain.getExtent().height);
    viewport.setMinDepth(0.0f).setMaxDepth(1.0f);
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.setOffset({0, 0});
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
                m_pipeline.getLayout(), 0,
                mat->getDescriptorSet(m_currentFrame), nullptr);

            obj->mesh->drawSubmesh(commandBuffer, sm);
        }
    }

    commandBuffer.endRenderPass();
    commandBuffer.end();
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

void Renderer::cleanup()
{
    if (m_cleaned) return;
    m_cleaned = true;

    m_context.getDevice().waitIdle();

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
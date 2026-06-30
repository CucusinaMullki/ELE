#include "Engine/Vulkan/Renderer.h"

void Renderer::init(GLFWwindow* window)
{
    m_context.initialize(window);
    m_swapchain.initialize(m_context, window);
    m_renderPass.initialize(m_context.getDevice(), m_swapchain.getImageFormat());
    m_pipeline.initialize(m_context.getDevice(), m_swapchain.getExtent(), m_renderPass.getRenderPass());
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass());

    std::cout << "Swapchain images: " << m_swapchain.getImageViews().size() << std::endl;
    std::cout << "Framebuffers: " << m_framebuffers.getFramebuffers().size() << std::endl;

    auto indices = m_context.GetQueueFamilyIndices();
    m_commandManager.intialize(m_context.getDevice(), indices.graphicFamily.value(), MAX_FRAMES_IN_FLIGHT);

    size_t imageCount = m_swapchain.getImageViews().size();

    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(imageCount);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_imageAvailableSemaphores[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
        m_inFlightFences[i] = m_context.getDevice().createFenceUnique(fenceInfo);
    }

    for (size_t i = 0; i < imageCount; i++)
    {
        m_renderFinishedSemaphores[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
    }
}

void Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
{
    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    vk::ClearValue clearColor{ vk::ClearColorValue{ std::array<float,4>{ 0.0f, 0.0f, 1.0f, 1.0f } } };

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.setRenderPass(m_renderPass.getRenderPass());
    renderPassInfo.setFramebuffer(m_framebuffers.getFramebuffers()[imageIndex].get());
    renderPassInfo.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
    renderPassInfo.setClearValues(clearColor);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
    commandBuffer.draw(3, 1, 0, 0);
    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Renderer::drawFrame()
{
    auto device = m_context.getDevice();
    auto fence  = m_inFlightFences[m_currentFrame].get();

    device.waitForFences(fence, true, UINT64_MAX);

    uint32_t imageIndex;
    try
    {
        auto acquireResult = device.acquireNextImageKHR(
            m_swapchain.getSwapChain(),
            UINT64_MAX,
            m_imageAvailableSemaphores[m_currentFrame].get(),
            nullptr
        );
        imageIndex = acquireResult.value;
    }
    catch (const vk::OutOfDateKHRError&)
    {
        recreateSwapchain();
        return;
    }

    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        device.waitForFences(m_imagesInFlight[imageIndex], true, UINT64_MAX);
    }
    m_imagesInFlight[imageIndex] = fence;

    device.resetFences(fence);

    auto cmd = m_commandManager.getCommandBuffer(m_currentFrame);
    cmd.reset();
    recordCommandBuffer(cmd, imageIndex);

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo{};
    submitInfo.setWaitSemaphores(m_imageAvailableSemaphores[m_currentFrame].get());
    submitInfo.setWaitDstStageMask(waitStage);
    submitInfo.setCommandBuffers(cmd);
    submitInfo.setSignalSemaphores(m_renderFinishedSemaphores[imageIndex].get());

    m_context.getGraphicQueue().submit(submitInfo, fence);

    auto swapchain = m_swapchain.getSwapChain();

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(m_renderFinishedSemaphores[imageIndex].get());
    presentInfo.setSwapchains(swapchain);
    presentInfo.setImageIndices(imageIndex);

    try
    {
        auto presentResult = m_context.getPresentQueue().presentKHR(presentInfo);
        if (presentResult == vk::Result::eSuboptimalKHR)
        {
            recreateSwapchain();
        }
    }
    catch (const vk::OutOfDateKHRError&)
    {
        recreateSwapchain();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::run(GLFWwindow* window)
{
    m_window = window;
    init(window);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawFrame();
    }

    m_context.getDevice().waitIdle();
}

void Renderer::cleanup()
{
    m_inFlightFences.clear();
    m_renderFinishedSemaphores.clear();
    m_imageAvailableSemaphores.clear();
    m_commandManager.cleanup();
    m_framebuffers.cleanup();
    m_pipeline.cleanup();
    m_renderPass.cleanup();
    m_swapchain.cleanup();
    m_context.shutdown();
}

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::recreateSwapchain()
{
    m_context.getDevice().waitIdle();

    m_framebuffers.cleanup();
    m_swapchain.recreate(m_context, m_window);
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass());

    size_t imageCount = m_swapchain.getImageViews().size();
    if (m_renderFinishedSemaphores.size() != imageCount)
    {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        m_renderFinishedSemaphores.resize(imageCount);
        for (size_t i = 0; i < imageCount; i++)
        {
            m_renderFinishedSemaphores[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
        }
        m_imagesInFlight.assign(imageCount, VK_NULL_HANDLE);
    }
}
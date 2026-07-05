#include "Vulkan/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Core/Log.h"
#include "Core/Model.h"
#include <chrono>

void Renderer::init(GLFWwindow* window)
{
    m_context.initialize(window);
    m_swapchain.initialize(m_context, window);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_renderPass.initialize(m_context.getDevice(), m_swapchain.getImageFormat(), m_depthBuffer.getFormat());
    m_pipeline.initialize(m_context.getDevice(), m_renderPass.getRenderPass(), "shader", MAX_FRAMES_IN_FLIGHT);

    auto indices2 = m_context.GetQueueFamilyIndices();
    m_commandManager.intialize(m_context.getDevice(), indices2.graphicFamily.value(), MAX_FRAMES_IN_FLIGHT);

    m_uniformBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);

    m_albedo.initialize(m_context.getDevice(), m_context.getPhysicalDevice(),
        m_commandManager.getCommandPool(), m_context.getGraphicQueue(), "assets/textures/albedo.jpg");
    m_normal.initialize(m_context.getDevice(), m_context.getPhysicalDevice(),
        m_commandManager.getCommandPool(), m_context.getGraphicQueue(), "assets/textures/normal.jpg");
    m_metallic.initialize(m_context.getDevice(), m_context.getPhysicalDevice(),
        m_commandManager.getCommandPool(), m_context.getGraphicQueue(), "assets/textures/metallic.jpg");
    m_roughness.initialize(m_context.getDevice(), m_context.getPhysicalDevice(),
        m_commandManager.getCommandPool(), m_context.getGraphicQueue(), "assets/textures/roughness.jpg");

    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        bufferInfos.push_back(m_uniformBuffer.getDescriptorInfo(i));

    m_pipeline.updateDescriptorSets(m_context.getDevice(), bufferInfos,
        m_albedo.getImageView(), m_albedo.getSampler(),
        m_normal.getImageView(), m_normal.getSampler(),
        m_metallic.getImageView(), m_metallic.getSampler(),
        m_roughness.getImageView(), m_roughness.getSampler());

    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    Model model;
    model.load("assets/models/cube.obj");

    m_vertexBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), model.getVertices());
    m_indexBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), model.getIndices());

    LOG("Swapchain images: " << m_swapchain.getImageViews().size());
    LOG("FrameBuffers: " << m_framebuffers.getFramebuffers().size());

    m_input.initialize(window);
    m_camera.initialize(glm::vec3(0.0f, 0.0f, 3.0f));
    m_camera.setAspect((float)m_swapchain.getExtent().width / m_swapchain.getExtent().height);

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

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.5f, 0.0f, 0.5f, 1.0f } });
    clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.setRenderPass(m_renderPass.getRenderPass());
    renderPassInfo.setFramebuffer(m_framebuffers.getFramebuffers()[imageIndex].get());
    renderPassInfo.setRenderArea(vk::Rect2D{ {0, 0}, m_swapchain.getExtent() });
    renderPassInfo.setClearValues(clearValues);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());

    commandBuffer.bindDescriptorSets(
       vk::PipelineBindPoint::eGraphics,
        m_pipeline.getLayout(),
        0,
        m_pipeline.getDescriptorSet(m_currentFrame),
        nullptr
    );

    vk::Viewport viewport{};
    viewport.setX(0.0f);
    viewport.setY(0.0f);
    viewport.setWidth(static_cast<float>(m_swapchain.getExtent().width));
    viewport.setHeight(static_cast<float>(m_swapchain.getExtent().height));
    viewport.setMinDepth(0.0f);
    viewport.setMaxDepth(1.0f);
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.setOffset({0, 0});
    scissor.setExtent(m_swapchain.getExtent());
    commandBuffer.setScissor(0, scissor);

    vk::Buffer vertexBuffers[] = { m_vertexBuffer.getBuffer() };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);

    commandBuffer.bindIndexBuffer(m_indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);

    commandBuffer.drawIndexed(m_indexBuffer.getIndexCount(), 1, 0, 0, 0);
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

    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    MVP mvp{};
    mvp.model = glm::mat4(1.0f);
    mvp.normalMatrix = glm::transpose(glm::inverse(mvp.model));
    mvp.view = m_camera.getViewMatrix();
    mvp.proj = m_camera.getProjectionMatrix();
    mvp.camPos = glm::vec4(m_camera.getPosition(), 1.0f);
    mvp.lightPos = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    mvp.lightColor = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) * 3.0f;  
    m_uniformBuffer.update(m_currentFrame, mvp);

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

    float currentFrameTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentFrameTime - m_lastFrameTime;
    m_lastFrameTime = currentFrameTime;

    m_input.update();

    glm::vec3 moveDir(0.0f);
    if (m_input.isKeyPressed(Key::W)) moveDir += m_camera.getFront();
    if (m_input.isKeyPressed(Key::S)) moveDir -= m_camera.getFront();
    if (m_input.isKeyPressed(Key::A)) moveDir -= m_camera.getRight();
    if (m_input.isKeyPressed(Key::D)) moveDir += m_camera.getRight();
    if (m_input.isKeyPressed(Key::E)) moveDir += m_camera.getUp();
    if (m_input.isKeyPressed(Key::Q)) moveDir -= m_camera.getUp();

    if (glm::length(moveDir) > 0.0f)
        m_camera.processKeyboard(glm::normalize(moveDir), deltaTime);

    m_camera.processMouse(m_input.getMouseDeltaX(), m_input.getMouseDeltaY());

    try
    {
        auto presentResult = m_context.getPresentQueue().presentKHR(presentInfo);
        if (presentResult == vk::Result::eSuboptimalKHR || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapchain();
        }
    }
    catch (const vk::OutOfDateKHRError&)
    {
        m_framebufferResized = false;
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
    if (m_cleaned) return; 
    m_cleaned = true;

    m_context.getDevice().waitIdle();
    
    m_albedo.cleanup();
    m_normal.cleanup();
    m_metallic.cleanup();
    m_roughness.cleanup();
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
    m_depthBuffer.cleanup();
    m_uniformBuffer.cleanup();
    m_imagesInFlight.clear();
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
    m_depthBuffer.cleanup();
    m_swapchain.recreate(m_context, m_window);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    m_camera.setAspect(
        (float)m_swapchain.getExtent().width / m_swapchain.getExtent().height
    );

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
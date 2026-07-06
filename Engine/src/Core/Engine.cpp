#include "Core/Engine.h"
#include "Vulkan/PushConstant.h"
#include "Core/Log.h"
#include <chrono>

void Engine::initialize(int width, int height, const std::string& title)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int, int) {
        auto* e = static_cast<Engine*>(glfwGetWindowUserPointer(win));
        e->onFramebufferResize();
    });

    initVulkan();
}

void Engine::initVulkan()
{
    m_context.initialize(m_window);
    m_swapchain.initialize(m_context, m_window);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_renderPass.initialize(m_context.getDevice(), m_swapchain.getImageFormat(), m_depthBuffer.getFormat());
    m_pipeline.initialize(m_context.getDevice(), m_renderPass.getRenderPass(), "shader", MAX_FRAMES_IN_FLIGHT);

    auto indices = m_context.GetQueueFamilyIndices();
    m_commandManager.intialize(m_context.getDevice(), indices.graphicFamily.value(), MAX_FRAMES_IN_FLIGHT);

    m_resource.initialize(m_context.getDevice(), m_context.getPhysicalDevice(),
        m_commandManager.getCommandPool(), m_context.getGraphicQueue());

    m_uniformBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), MAX_FRAMES_IN_FLIGHT);

    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    m_input.initialize(m_window);
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
        m_renderFinishedSemaphores[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
}

std::shared_ptr<Material> Engine::loadMaterial(const std::string& albedo, const std::string& normal,
    const std::string& metallic, const std::string& roughness)
{
    auto material = m_resource.loadMaterial(albedo, normal, metallic, roughness);

    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        bufferInfos.push_back(m_uniformBuffer.getDescriptorInfo(i));

    material->createDescriptorSet(m_context.getDevice(),
        m_pipeline.getDescriptorPool(),
        m_pipeline.getDescriptorSetLayout(),
        bufferInfos, MAX_FRAMES_IN_FLIGHT);

    return material;
}

void Engine::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        drawFrame();
    }
    m_context.getDevice().waitIdle();
    cleanup();
}

void Engine::drawFrame()
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
            m_imageAvailableSemaphores[m_currentFrame].get(), nullptr);
        imageIndex = acquireResult.value;
    }
    catch (const vk::OutOfDateKHRError&)
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

void Engine::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
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
    viewport.setX(0.0f).setY(0.0f)
            .setWidth((float)m_swapchain.getExtent().width)
            .setHeight((float)m_swapchain.getExtent().height)
            .setMinDepth(0.0f).setMaxDepth(1.0f);
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor{};
    scissor.setOffset({0, 0});
    scissor.setExtent(m_swapchain.getExtent());
    commandBuffer.setScissor(0, scissor);

    for (const auto& obj : m_scene.getObjects())
    {
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            m_pipeline.getLayout(), 0,
            obj->material->getDescriptorSet(m_currentFrame), nullptr);

        PushConstant pc{};
        pc.model = obj->transform.getMatrix();
        pc.normalMatrix = obj->transform.getNormalMatrix();
        commandBuffer.pushConstants(m_pipeline.getLayout(),
            vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstant), &pc);

        obj->mesh->bind(commandBuffer);
        obj->mesh->draw(commandBuffer);
    }

    commandBuffer.endRenderPass();
    commandBuffer.end();
}

void Engine::recreateSwapchain()
{
    m_context.getDevice().waitIdle();
    m_framebuffers.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.recreate(m_context, m_window);
    m_depthBuffer.initialize(m_context.getDevice(), m_context.getPhysicalDevice(), m_swapchain.getExtent());
    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());
    m_camera.setAspect((float)m_swapchain.getExtent().width / m_swapchain.getExtent().height);

    size_t imageCount = m_swapchain.getImageViews().size();
    if (m_renderFinishedSemaphores.size() != imageCount)
    {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        m_renderFinishedSemaphores.resize(imageCount);
        for (size_t i = 0; i < imageCount; i++)
            m_renderFinishedSemaphores[i] = m_context.getDevice().createSemaphoreUnique(semaphoreInfo);
        m_imagesInFlight.assign(imageCount, VK_NULL_HANDLE);
    }
}

void Engine::cleanup()
{
    if (m_cleaned) return;
    m_cleaned = true;

    m_context.getDevice().waitIdle();

    m_resource.cleanup();
    m_uniformBuffer.cleanup();

    m_imagesInFlight.clear();
    m_inFlightFences.clear();
    m_renderFinishedSemaphores.clear();
    m_imageAvailableSemaphores.clear();
    m_commandManager.cleanup();
    m_framebuffers.cleanup();
    m_pipeline.cleanup();
    m_renderPass.cleanup();
    m_depthBuffer.cleanup();
    m_swapchain.cleanup();
    m_context.shutdown();
}


void Engine::shutdown()
{
    cleanup();
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}
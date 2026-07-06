#include "Vulkan/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Vulkan/PushConstant.h"
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

    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        bufferInfos.push_back(m_uniformBuffer.getDescriptorInfo(i));

    m_framebuffers.initialize(m_context.getDevice(), m_swapchain, m_renderPass.getRenderPass(), m_depthBuffer.getImageView());

    m_cubeMesh = std::make_shared<Mesh>();
    m_cubeMesh->load(m_context.getDevice(), m_context.getPhysicalDevice(), "assets/models/cube.obj");

    m_defaultMaterial = std::make_shared<Material>();
    m_defaultMaterial->load(m_context.getDevice(), m_context.getPhysicalDevice(),
        m_commandManager.getCommandPool(), m_context.getGraphicQueue(),
        "assets/textures/albedo.jpg", "assets/textures/normal.jpg",
        "assets/textures/metallic.jpg", "assets/textures/roughness.jpg");

    m_pipeline.updateDescriptorSets(m_context.getDevice(), bufferInfos,
        m_defaultMaterial->getAlbedoView(), m_defaultMaterial->getAlbedoSampler(),
        m_defaultMaterial->getNormalView(), m_defaultMaterial->getNormalSampler(),
        m_defaultMaterial->getMetallicView(), m_defaultMaterial->getMetallicSampler(),
        m_defaultMaterial->getRoughnessView(), m_defaultMaterial->getRoughnessSampler());

    auto& obj1 = m_scene.addObject();
    obj1.mesh = m_cubeMesh;
    obj1.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);

    auto& obj2 = m_scene.addObject();
    obj2.mesh = m_cubeMesh;
    obj2.transform.position = glm::vec3(3.0f, 0.0f, 0.0f);

    auto& obj3 = m_scene.addObject();
    obj3.mesh = m_cubeMesh;
    obj3.transform.position = glm::vec3(-3.0f, 0.0f, 0.0f);
    obj3.transform.scale = glm::vec3(0.5f);

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
    clearValues[0].setColor(vk::ClearColorValue{ std::array<float,4>{ 0.02f, 0.02f, 0.03f, 1.0f } });
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

    // Цикл по объектам сцены
    for (const auto& obj : m_scene.getObjects())
    {
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
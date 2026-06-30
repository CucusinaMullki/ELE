#pragma once

#include "Engine/Vulkan/VulkanContext.h"
#include "Engine/Vulkan/SwapChain.h"
#include "Engine/Vulkan/RenderPass.h"
#include "Engine/Vulkan/Pipeline.h"
#include "Engine/Vulkan/FrameBuffers.h"
#include "Engine/Vulkan/CommandManager.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <vector>

class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    void run(GLFWwindow* window);

private:
    void init(GLFWwindow* window);
    void drawFrame();
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
    void cleanup();
    void recreateSwapchain();

    const int MAX_FRAMES_IN_FLIGHT = 2;

    VulkanContext m_context;
    SwapChain m_swapchain;
    RenderPass m_renderPass;
    Pipeline m_pipeline;
    Framebuffers m_framebuffers;
    CommandManager m_commandManager;
    GLFWwindow* m_window = nullptr;

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    size_t m_currentFrame = 0;
};
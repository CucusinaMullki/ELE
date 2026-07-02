#pragma once

#include "Vulkan/VulkanContext.h"
#include "Vulkan/SwapChain.h"
#include "Vulkan/RenderPass.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/FrameBuffers.h"
#include "Vulkan/CommandManager.h"
#include "Vulkan/UniformBuffer.h"
#include "Vulkan/VertexBuffer.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <vector>

class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    void run(GLFWwindow* window);
    void cleanup();

private:
    void init(GLFWwindow* window);
    void drawFrame();
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
    void recreateSwapchain();

    const int MAX_FRAMES_IN_FLIGHT = 2;
    bool m_cleaned = false;

    VulkanContext m_context;
    SwapChain m_swapchain;
    RenderPass m_renderPass;
    Pipeline m_pipeline;
    Framebuffers m_framebuffers;
    CommandManager m_commandManager;
    UniformBuffer m_uniformBuffer;
    GLFWwindow* m_window = nullptr;
    VertexBuffer m_vertexBuffer;

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    size_t m_currentFrame = 0;
};
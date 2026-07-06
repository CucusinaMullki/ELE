#pragma once
#include "Vulkan/VulkanContext.h"
#include "Vulkan/SwapChain.h"
#include "Vulkan/RenderPass.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/FrameBuffers.h"
#include "Vulkan/CommandManager.h"
#include "Vulkan/UniformBuffer.h"
#include "Vulkan/DepthBuffer.h"
#include "Core/Input.h"
#include "Core/Camera.h"
#include "Scene/Scene.h"
#include <GLFW/glfw3.h>
#include "Core/ResourceManager.h"
#include <memory>
#include <string>

class Engine
{
public:
    void initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();

    std::shared_ptr<Mesh> loadMesh(const std::string& path)
    {
        return m_resource.loadMesh(path);
    }
    
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
        const std::string& metallic, const std::string& roughness);

    Scene& getScene() { return m_scene; }
    Camera& getCamera() { return m_camera; }

    void onFramebufferResize() { m_framebufferResized = true; }

private:
    void initVulkan();
    void drawFrame();
    void recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex);
    void recreateSwapchain();
    void cleanup();

    const int MAX_FRAMES_IN_FLIGHT = 2;
    bool m_cleaned = false;
    bool m_framebufferResized = false;

    GLFWwindow* m_window = nullptr;

    VulkanContext  m_context;
    SwapChain      m_swapchain;
    RenderPass     m_renderPass;
    Pipeline       m_pipeline;
    Framebuffers   m_framebuffers;
    CommandManager m_commandManager;
    UniformBuffer  m_uniformBuffer;
    DepthBuffer    m_depthBuffer;
    Input          m_input;
    Camera         m_camera;
    Scene          m_scene;
    ResourceManager m_resource;

    std::shared_ptr<Material> m_boundMaterial;

    float m_lastFrameTime = 0.0f;
    size_t m_currentFrame = 0;

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence>     m_inFlightFences;
    std::vector<vk::Fence>           m_imagesInFlight;    
};
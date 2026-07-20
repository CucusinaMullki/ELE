#pragma once

#include "Vulkan/VulkanContext.h"
#include "Vulkan/SwapChain.h"
#include "Vulkan/RenderPass.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/FrameBuffers.h"
#include "Vulkan/CommandManager.h"
#include "Vulkan/UniformBuffer.h"
#include "Vulkan/LightBuffer.h"
#include "Vulkan/ShadowMap.h"
#include "Vulkan/ShadowRenderPass.h"
#include "Vulkan/PipelineCache.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/ShadowMap.h"
#include "Scene/Scene.h"
#include "Core/Camera.h"
#include <GLFW/glfw3.h>

class Renderer
{
public:
    void initialize(GLFWwindow* window);
    void drawScene(const Scene& scene, const Camera& camera);
    void cleanup();

    void onResize() { m_framebufferResized = true; }

    void setLights(const LightData& lights) { m_pendingLights = lights; }

    vk::Device getDevice() const { return m_context.getDevice(); }
    vk::PhysicalDevice getPhysicalDevice() const { return m_context.getPhysicalDevice(); }
    vk::CommandPool getCommandPool() const { return m_commandManager.getCommandPool(); }
    vk::Queue getGraphicsQueue() const { return m_context.getGraphicQueue(); }
    vk::DescriptorPool getDescriptorPool() const { return m_pipeline.getDescriptorPool(); }
    vk::DescriptorSetLayout getDescriptorSetLayout() const { return m_pipeline.getDescriptorSetLayout(); }
    std::vector<vk::DescriptorBufferInfo> getLightInfos() const;
    vk::ImageView getShadowView() const { return m_shadowMap.getImageView(); }
    vk::Sampler getShadowSampler() const { return m_shadowMap.getSampler(); }

    uint32_t getFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
    float getAspect() const;

    std::vector<vk::DescriptorBufferInfo> getUboInfos() const;

private:
    void recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex,
        const Scene& scene, const Camera& camera);
    
    void recreateSwapchain();

    static const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    GLFWwindow* m_window = nullptr;
    bool m_cleaned = false;
    bool m_framebufferResized = false;
    size_t m_currentFrame = 0;
    
    VulkanContext m_context;
    SwapChain m_swapchain;
    RenderPass m_renderPass;
    Pipeline m_pipeline;
    Framebuffers m_framebuffers;
    CommandManager m_commandManager;
    UniformBuffer m_uniformBuffer;
    DepthBuffer m_depthBuffer;

    PipelineCache m_pipelineCache;

    //Shadows
    ShadowMap m_shadowMap;
    ShadowRenderPass m_shadowRenderPass;
    Pipeline m_shadowPipeline;
    vk::UniqueFramebuffer m_shadowFramebuffer;
    glm::mat4 m_lightSpaceMatrix{1.0f};

    LightBuffer m_lightBuffer;
    LightData m_lightData{};
    
    LightData m_pendingLights{};

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphore;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphore;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;
};
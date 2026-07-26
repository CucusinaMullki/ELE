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
#include "Vulkan/HdrRenderPass.h"
#include "Vulkan/HdrTarget.h"
#include "Vulkan/BloomTarget.h"
#include "Vulkan/PostRenderPass.h"
#include "Vulkan/CubeShadowMap.h"
#include "Vulkan/PointShadowRenderPass.h"
#include "Vulkan/BloomChain.h"
#include "Vulkan/PointShadowPush.h"
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
    vk::ImageView getPointShadowView() const { return m_pointShadowMap.getCubeView(); }
    vk::Sampler getPointShadowSampler() const { return m_pointShadowMap.getSampler(); }

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
    
    float m_exposure = 1.0f;

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

    //
    CubeShadowMap m_pointShadowMap;
    PointShadowRenderPass m_pointShadowRenderPass;
    std::array<vk::UniqueFramebuffer, 6> m_pointShadowFramebuffers;
    Pipeline m_pointShadowPipeline;
    std::array<glm::mat4, 6> m_pointLightViewProj;
    float m_pointShadowFarPlane = 25.0f;
    void updatePointLightMatrices(glm::vec3 lightPos);
    bool m_hasPointLightShadow = false;
    glm::vec3 m_currentPointLightPos{0.0f};

    //Bloom
    BloomTarget m_brighTarget;
    BloomTarget m_blurTargetA;
    BloomTarget m_blurTargetB;
    PostRenderPass m_postRenderPass;

    vk::UniqueFramebuffer m_brightFramebuffer;
    vk::UniqueFramebuffer m_blurFramebufferA;
    vk::UniqueFramebuffer m_blurFramebufferB;

    Pipeline m_brightPipeline;
    Pipeline m_blurPipeline;

    std::vector<vk::DescriptorSet> m_brightDescriptorSets;
    std::vector<vk::DescriptorSet> m_blurDescriptorSetsA;
    std::vector<vk::DescriptorSet> m_blurDescriptorSetsB;

    BloomChain m_bloomChain;
    std::array<vk::UniqueFramebuffer, BloomChain::MIP_COUNT> m_bloomFramebuffers;

    Pipeline m_downsamplePipeline;
    Pipeline m_upsamplePipeline;

    std::vector<vk::DescriptorSet> m_downsampleDescriptorSets[BloomChain::MIP_COUNT];
    std::vector<vk::DescriptorSet> m_upsampleDescriptorSets[BloomChain::MIP_COUNT];
    
    PostRenderPass m_postRenderPassLoad;
    
    float m_bloomThreshold = 4.0f;

    float m_contrast = 1.0f;

    //HDR
    HdrTarget m_hdrTarget;
    HdrRenderPass m_hdrRenderPass;
    vk::UniqueFramebuffer m_hdrFramebuffer;
    Pipeline m_postPipeline;
    std::vector<vk::DescriptorSet> m_postDescriptorSets;

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphore;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphore;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;
};
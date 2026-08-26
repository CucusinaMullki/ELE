#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
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
#include "Vulkan/ShadowAtlas.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/ShadowMap.h"
#include "Vulkan/HdrRenderPass.h"
#include "Vulkan/HdrTarget.h"
#include "Vulkan/BloomTarget.h"
#include "Vulkan/PostRenderPass.h"
#include "Vulkan/CubeShadowMap.h"
#include "Vulkan/PointShadowRenderPass.h"
#include "Vulkan/BloomChain.h"
#include "Vulkan/AntiAliasing.h"
#include "Vulkan/NoAA.h"
#include "Vulkan/VelocityTarget.h"
#include "Vulkan/HistoryBuffer.h"
#include "Vulkan/GodRayTarget.h"
#include "Core/GodRaySettings.h"
#include "Vulkan/PointShadowPush.h"
#include "Scene/Scene.h"
#include "Vulkan/LdrTarget.h"
#include "Vulkan/PostRenderPass.h"
#include "Vulkan/FogCameraBuffer.h"
#include "Core/FogSettings.h"
#include "Vulkan/FXAA.h"
#include "Vulkan/DecalBox.h"
#include "Vulkan/DecalCameraBuffer.h"
#include "Scene/ScreenSpaceDecal.h"
#include "Core/Camera.h"
#include <GLFW/glfw3.h>
#include <future>
#include <thread>

class Renderer
{
public:
    void initialize(GLFWwindow* window);
    void drawScene(const Scene& scene, const Camera& camera);
    void cleanup();

    void onResize() { m_framebufferResized = true; }

    void setLights(const LightData& lights) { m_pendingLights = lights; }

    void setAAMethod(AAMethod method);

    void setExposure(float exp) { m_exposure = exp; }

    void setFogSettings(const FogSettings& settings) { m_fogSettings = settings; }

    void setGodRaySettings(const GodRaySettings& settings) { m_godRaySettings = settings; }

    void setTotalTime(float t) { m_totalTime = t; }

    void addScreenSpaceDecal(std::shared_ptr<ScreenSpaceDecal> decal) { m_screenSpaceDecals.push_back(decal); }

    void initImGui(GLFWwindow* window);
    void beginImGuiFrame();
    void renderImGui(vk::CommandBuffer commandBuffer);
    void cleanupImGui();

    vk::Device getDevice() const { return m_context.getDevice(); }
    const FogSettings& getFogSettings() const { return m_fogSettings;}
    vk::PhysicalDevice getPhysicalDevice() const { return m_context.getPhysicalDevice(); }
    vk::CommandPool getCommandPool() const { return m_commandManager.getCommandPool(); }
    vk::Queue getGraphicsQueue() const { return m_context.getGraphicQueue(); }
    vk::DescriptorPool getDescriptorPool() const { return m_pipeline.getDescriptorPool(); }
    vk::DescriptorSetLayout getDescriptorSetLayout() const { return m_pipeline.getDescriptorSetLayout(); }
    std::vector<vk::DescriptorBufferInfo> getLightInfos() const;
    vk::Sampler getShadowAtlasSampler() const { return m_shadowAtlas.getSampler(); }
    vk::ImageView getShadowAtlasView() const { return m_shadowAtlas.getImageView(); }
    vk::ImageView getShadowView() const { return m_shadowMap.getImageView(); }
     vk::Sampler getShadowSampler() const { return m_shadowMap.getSampler(); }
    vk::DescriptorPool getDecalDescriptorPool() const { return m_decalPipeline.getDescriptorPool(); }
    vk::DescriptorSetLayout getDecalDescriptorSetLayout() const { return m_decalPipeline.getDescriptorSetLayout(); }
    vk::ImageView getDepthView() const { return m_depthBuffer.getImageView(); }
    vk::Sampler getDepthSampler() const { return m_depthBuffer.getSampler(); }
    const LightBuffer& getLightBuffer() const { return m_lightBuffer; }
    const DecalCameraBuffer& getDecalCameraBuffer() const { return m_decalCameraBuffer; }
    const GodRaySettings& getGodRaySettings() const{ return m_godRaySettings; }
    uint32_t getFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
    vk::DescriptorSetLayout getSkinnedDescriptorSetLayout() const { return m_skinnedPipeline.getDescriptorSetLayout(); }
    vk::DescriptorPool getSkinnedDescriptorPool() const { return m_skinnedPipeline.getDescriptorPool(); }
    float getAspect() const;

    std::vector<const GameObject*> parallelCull(const Scene& scene, const std::array<glm::vec4, 6>& planes) const;

    bool isVisible(const GameObject& obj, const std::array<glm::vec4, 6>& planes) const;

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

    Pipeline m_skinnedPipeline;

    //Shadows
    ShadowMap m_shadowMap;
    ShadowRenderPass m_shadowRenderPass;
    Pipeline m_shadowPipeline;
    vk::UniqueFramebuffer m_shadowFramebuffer;
    glm::mat4 m_lightSpaceMatrix{1.0f};

    LightBuffer m_lightBuffer;
    LightData m_lightData{};
    
    LightData m_pendingLights{};

    // Point light shadows
    PointShadowRenderPass m_pointShadowRenderPass;
    Pipeline m_pointShadowPipeline;
    float m_pointShadowFarPlane = 25.0f;
    bool m_hasPointLightShadow = false;

    static constexpr int NUM_POINT_SHADOWS = 3;


    ShadowAtlas m_shadowAtlas;
    vk::UniqueFramebuffer m_shadowAtlasFramebuffer;

    std::array<std::array<glm::mat4, 6>, NUM_POINT_SHADOWS> m_pointLightViewProjArr;
    void updatePointLightMatrices(int slot, glm::vec3 lightPos);
    int m_numActivePointShadows = 0;
    std::array<glm::vec3, NUM_POINT_SHADOWS> m_activePointLightPos;
    std::array<int, NUM_POINT_SHADOWS> m_activePointLightIndex;

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

    std::vector<vk::DescriptorSet> m_brightDescriptorSets[2];
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
    std::vector<vk::DescriptorSet> m_postDescriptorSets[2];

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphore;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphore;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    //Anti Aliasing
    AAMethod m_aaMethod = AAMethod::None;
    std::unique_ptr<AntiAliasing> m_antiAliasing;
    LdrTarget m_ldrTarget;
    PostRenderPass m_ldrRenderPass;
    vk::UniqueFramebuffer m_ldrFramebuffer;
    FXAA m_fxaa;

    std::array<glm::vec2, 16> m_halfSequence;
    uint32_t m_frameIndex = 0;

    glm::vec2 getJitter() const;

    VelocityTarget m_velocityTarget;

    HistoryBuffer m_historyBuffer;
    std::array<vk::UniqueFramebuffer, 2> m_taaFramebuffers;
    Pipeline m_taaPipeline;
    std::vector<vk::DescriptorSet> m_taaDescriptorSets[2];
    uint32_t m_historyIndex = 0;
    float m_taaBlendFactor = 0.9f;

    std::vector<vk::DescriptorSet> m_brightDescriptorSetsRaw;
    std::vector<vk::DescriptorSet> m_postDescriptorSetsRaw;

    //Decal
    Pipeline m_decalPipeline;
    DecalBox m_decalBox;
    DecalCameraBuffer m_decalCameraBuffer;
    std::vector<std::shared_ptr<ScreenSpaceDecal>> m_screenSpaceDecals;
    PostRenderPass m_hdrDecalRenderPass;
    vk::UniqueFramebuffer m_hdrDecalFramebuffer;

    //Volumetric fog
    Pipeline m_fogPipeline;
    FogCameraBuffer m_fogCameraBuffer;
    std::vector<vk::DescriptorSet> m_fogDescriptorSets;
    FogSettings m_fogSettings;
    void createFogDescriptorSets();
    float m_totalTime = 0.0f;

    //God rays
    GodRayTarget m_godRayMaskTarget;
    GodRayTarget m_godRayBlurTarget;
    vk::UniqueFramebuffer m_godRayMaskFramebuffer;
    vk::UniqueFramebuffer m_godRayBlurFramebuffer;

    Pipeline m_godRayMaskPipeline;
    Pipeline m_godRayBlurPipeline;
    Pipeline m_godRayCompositePipeline;

    std::vector<vk::DescriptorSet> m_godRayMaskDescriptorSets;
    std::vector<vk::DescriptorSet> m_godRayBlurDescriptorSets;
    std::vector<vk::DescriptorSet> m_godRayCompositeDescriptorSets;

    GodRaySettings m_godRaySettings;

    glm::mat4 m_prevViewProj = glm::mat4(1.0f);

    std::array<glm::vec4, 6> extractFrustumPlanes(const glm::mat4& viewProj) const;
    bool isSubmeshVisible(const glm::mat4& modelMatrix, const Submesh& sm, const std::array<glm::vec4, 6>& planes) const;

    vk::UniqueDescriptorPool m_imguiDescriptorPool;
};
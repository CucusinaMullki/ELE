#pragma once
#include "ASH/ASH.h"
#include "Vulkan/CubeTexture.h"
#include "Vulkan/HdrTexture.h"
#include "Vulkan/DecalBox.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PostRenderPass.h"
#include <memory>
#include <array>
#include <string>

class EnvironmentBaker
{
public:
    void initialize(ASH::Device* device);
    void cleanup();

    void bakeEnvironmentCubemap(ASH::Device* device, const std::string& hdrPath, CubeTexture& outCubemap);
    void bakeIrradianceMap(ASH::Device* device, CubeTexture& sourceEnvironment, CubeTexture& outIrradiance);

private:
    void renderCubeFacesWithPipeline(ASH::Device* device, ASH::Texture* srcTexture, ASH::Sampler* srcSampler,
        CubeTexture& target, uint32_t mipLevel, uint32_t faceSize, Pipeline& pipeline);

    DecalBox m_cubeMesh;
    PostRenderPass m_bakeRenderPass;
    Pipeline m_equirectToCubePipeline;
    Pipeline m_irradiancePipeline;
    bool m_initialized = false;
};

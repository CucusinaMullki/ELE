#pragma once
#include <glm/glm.hpp>
#include "Vulkan/Texture.h"
#include "Vulkan/DecalCameraBuffer.h"
#include "Vulkan/LightBuffer.h"
#include "ASH/ASH.h"
#include <vector>
#include <memory>

class ScreenSpaceDecal
{
public:
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 size{1.0f};

    float metallic = 0.0f;
    float roughness = 0.6f;

    void loadTexture(ASH::Device* device,
        const std::string& albedoPath,
        const std::string& opacityPath = "",
        const std::string& normalPath = "",
        const std::string& mrPath = "");
    void cleanup();

    glm::mat4 getModelMatrix() const;
    glm::mat4 getInvModelMatrix() const;
    ASH::DescriptorSet* getDescriptorSet(uint32_t frameIndex) const { return m_descriptorSets[frameIndex].get(); }

    void createDescriptorSet(ASH::Device* device, ASH::DescriptorSetLayout* layout,
        ASH::Texture* depthTexture, ASH::Sampler* depthSampler,
        const DecalCameraBuffer& cameraBuffer, const LightBuffer& lightBuffer, uint32_t frameCount);

private:
    Texture m_albedoTexture;
    Texture m_opacityTexture;
    Texture m_normalTexture;
    Texture m_mrTexture;
    std::vector<std::unique_ptr<ASH::DescriptorSet>> m_descriptorSets;
};
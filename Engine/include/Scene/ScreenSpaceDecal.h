#pragma once
#include <glm/glm.hpp>
#include "Vulkan/Texture.h"
#include "Vulkan/DecalCameraBuffer.h"
#include "Vulkan/LightBuffer.h"
#include <vulkan/vulkan.hpp>
#include <vector>

class ScreenSpaceDecal
{
public:
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 size{1.0f};

    float metallic = 0.0f;
    float roughness = 0.6f;

    void loadTexture(vk::Device device, vk::PhysicalDevice physicalDevice,
        vk::CommandPool pool, vk::Queue queue, 
        const std::string& albedoPath,
        const std::string& opacityPath = "",
        const std::string& normalPath = "",
        const std::string& mrPath = "");
    void cleanup();

    glm::mat4 getModelMatrix() const;
    glm::mat4 getInvModelMatrix() const;
    vk::DescriptorSet getDescriptorSet(uint32_t frameIndex) const { return m_descriptorSets[frameIndex]; }

    void createDescriptorSet(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout,
        vk::ImageView depthView, vk::Sampler depthSampler,
        const DecalCameraBuffer& cameraBuffer, const LightBuffer& lightBuffer, uint32_t frameCount);

private:
    Texture m_albedoTexture;
    Texture m_opacityTexture;
    Texture m_normalTexture;
    Texture m_mrTexture;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};
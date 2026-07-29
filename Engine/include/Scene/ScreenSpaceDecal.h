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
        vk::CommandPool pool, vk::Queue queue, const std::string& path);
    void cleanup();

    glm::mat4 getModelMatrix() const;
    glm::mat4 getInvModelMatrix() const;

    vk::ImageView getTextureView() const { return m_texture.getImageView(); }
    vk::Sampler getTextureSampler() const { return m_texture.getSampler(); }

    void createDescriptorSet(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout,
        vk::ImageView depthView, vk::Sampler depthSampler,
        const DecalCameraBuffer& cameraBuffer, const LightBuffer& lightBuffer, uint32_t frameCount);
    vk::DescriptorSet getDescriptorSet(uint32_t frame) const { return m_descriptorSets[frame]; }

private:
    Texture m_texture;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};
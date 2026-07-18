#pragma once
#include "Vulkan/Texture.h"
#include <string>

class Material
{
public:
    void load(vk::Device device, vk::PhysicalDevice physicalDevice,
        vk::CommandPool pool, vk::Queue queue,
        const std::string& albedoPath,
        const std::string& normalPath,
        const std::string& metallicPath,
        const std::string& roughnessPath);

    void loadFromPixels(vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool commandPool, vk::Queue queue,
        const unsigned char* albedoPx, int aw, int ah,
        const unsigned char* normalPx, int nw, int nh,
        const unsigned char* mrPx, int mw, int mh,
        const unsigned char* aoPx, int ow, int oh);

    vk::ImageView getAlbedoView() const { return m_albedo.getImageView(); }
    vk::Sampler getAlbedoSampler() const { return m_albedo.getSampler(); }
    vk::ImageView getNormalView() const { return m_normal.getImageView(); }
    vk::Sampler getNormalSampler() const { return m_normal.getSampler(); }
    vk::ImageView getMetallicRoughnessView() const { return m_metallicRoughness.getImageView(); }
    vk::Sampler getMetallicRoughnessSampler() const { return m_metallicRoughness.getSampler(); }
    vk::ImageView getAoView() const { return m_ao.getImageView(); }
    vk::Sampler getAoSampler() const { return m_ao.getSampler(); }
    

    void createDescriptorSet(vk::Device device, vk::DescriptorPool pool,
        vk::DescriptorSetLayout layout,
        const std::vector<vk::DescriptorBufferInfo>& uboInfos,
        uint32_t frameCount);
    
    vk::DescriptorSet getDescriptorSet(uint32_t frame) const { return m_descriptorSets[frame]; }

    void cleanup();
    
private:
    Texture m_albedo;
    Texture m_normal;
    Texture m_metallicRoughness;
    Texture m_ao;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};
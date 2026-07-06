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
    void cleanup();

    vk::ImageView getAlbedoView() const { return m_albedo.getImageView(); }
    vk::Sampler getAlbedoSampler() const { return m_albedo.getSampler(); }
    vk::ImageView getNormalView() const { return m_normal.getImageView(); }
    vk::Sampler getNormalSampler() const { return m_normal.getSampler(); }
    vk::ImageView getRoughnessView() const { return m_roughness.getImageView(); }
    vk::Sampler getRoughnessSampler() const { return m_roughness.getSampler(); }
    vk::ImageView getMetallicView() const { return m_metallic.getImageView(); }
    vk::Sampler getMetallicSampler() const { return m_metallic.getSampler(); }
    
private:
    Texture m_albedo;
    Texture m_normal;
    Texture m_metallic;
    Texture m_roughness;
};
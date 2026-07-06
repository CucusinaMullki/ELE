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

    void createDescriptorSet(vk::Device device, vk::DescriptorPool pool,
        vk::DescriptorSetLayout layout,
        const std::vector<vk::DescriptorBufferInfo>& uboInfos,
        uint32_t frameCount);
    
    vk::DescriptorSet getDescriptorSet(uint32_t frame) const { return m_descriptorSets[frame]; }

    void cleanup();
    
private:
    Texture m_albedo;
    Texture m_normal;
    Texture m_metallic;
    Texture m_roughness;

    std::vector<vk::DescriptorSet> m_descriptorSets; 
};
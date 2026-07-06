#include "Scene/Material.h"

void Material::load(vk::Device device, vk::PhysicalDevice physicalDevice,
        vk::CommandPool pool, vk::Queue queue,
        const std::string& albedoPath,
        const std::string& normalPath,
        const std::string& metallicPath,
        const std::string& roughnessPath)
{
    m_albedo.initialize(device, physicalDevice, pool, queue, albedoPath);
    m_normal.initialize(device, physicalDevice, pool, queue, normalPath);
    m_metallic.initialize(device, physicalDevice, pool, queue, metallicPath);
    m_roughness.initialize(device, physicalDevice, pool, queue, roughnessPath);
}

void Material::cleanup()
{
    m_albedo.cleanup();
    m_normal.cleanup();
    m_metallic.cleanup();
    m_roughness.cleanup();
}
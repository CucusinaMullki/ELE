#pragma once
#include "Scene/Material.h"
#include "Scene/Mesh.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>
#include <string>

class ResourceManager
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool commandPool, vk::Queue graphicsQueue);

    std::shared_ptr<Mesh> loadMesh(const std::string& path);
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
        const std::string& metallic, const std::string& roughness);

    void cleanup();

private:
    vk::Device         m_device;
    vk::PhysicalDevice m_physicalDevice;
    vk::CommandPool    m_commandPool;
    vk::Queue          m_graphicsQueue;

    std::vector<std::shared_ptr<Mesh>>     m_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;
};
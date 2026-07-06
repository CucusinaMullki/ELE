#include "Core/ResourceManager.h"

void ResourceManager::initialize(vk::Device device, vk::PhysicalDevice physicalDevice,
    vk::CommandPool commandPool, vk::Queue graphicsQueue)
{
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_commandPool = commandPool;
    m_graphicsQueue = graphicsQueue;
}

std::shared_ptr<Mesh> ResourceManager::loadMesh(const std::string& path)
{
    auto mesh = std::make_shared<Mesh>();
    mesh->load(m_device, m_physicalDevice, path);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Material> ResourceManager::loadMaterial(const std::string& albedo, const std::string& normal,
    const std::string& metallic, const std::string& roughness)
{
    auto material = std::make_shared<Material>();
    material->load(m_device, m_physicalDevice, m_commandPool, m_graphicsQueue,
        albedo, normal, metallic, roughness);
    m_materials.push_back(material);
    return material;
}

void ResourceManager::cleanup()
{
    for (auto& mesh : m_meshes)
        mesh->cleanup();
    for (auto& material : m_materials)
        material->cleanup();

    m_meshes.clear();
    m_materials.clear();
}
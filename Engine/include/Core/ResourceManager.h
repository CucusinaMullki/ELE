#pragma once
#include "Scene/Material.h"
#include "Scene/Mesh.h"
#include "ASH/ASH.h"
#include <vector>
#include <memory>
#include <string>

class ResourceManager
{
public:
    void initialize(ASH::Device* device);

    std::shared_ptr<Mesh> loadMesh(const std::string& path, bool keepCpuData = false);
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
        const std::string& metallic, const std::string& roughness);

    void registerMesh(std::shared_ptr<Mesh> m) { m_meshes.push_back(m); }
    void registerMaterial(std::shared_ptr<Material> m) { m_materials.push_back(m); }

    void cleanup();

private:
    ASH::Device* m_device = nullptr;

    std::vector<std::shared_ptr<Mesh>>     m_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;
};

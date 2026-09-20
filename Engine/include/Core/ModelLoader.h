#pragma once
#include "Vulkan/Vertex.h"
#include <vector>
#include <string>

struct ModelSubmesh
{
    int32_t materialIndex;
    uint32_t indexOffset;
    uint32_t indexCount;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
};

struct ModelMaterial
{
    std::string albedoPath;
    std::string normalPath;
    std::string metallicRoughnessPath;
    std::string aoPath;
};

class ModelLoader
{
public:
    void load(const std::string& path);

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<ModelSubmesh>& getSubmeshes() const { return m_submeshes; }
    const std::vector<ModelMaterial>& getMaterials() const { return m_materials; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<ModelSubmesh> m_submeshes;
    std::vector<ModelMaterial> m_materials;
};
#pragma once
#include "Scene/Skeleton.h"
#include "Vulkan/SkinnedVertex.h"
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

struct ModelBone
{
    std::string name;
    int32_t parentIndex;
    glm::mat4 inverseBindMatrix;
    glm::mat4 localBindMatrix;
};

class ModelLoader
{
public:
    void load(const std::string& path);

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<ModelSubmesh>& getSubmeshes() const { return m_submeshes; }
    const std::vector<ModelMaterial>& getMaterials() const { return m_materials; }
    const std::vector<ModelBone>& getBones() const { return m_bones; }
    bool hasSkeleton() const { return  !m_bones.empty(); }
    Skeleton buildSkeleton() const;
    const std::vector<SkinnedVertex>& getSkinnedVertices() const { return m_skinnedVertices; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<ModelSubmesh> m_submeshes;
    std::vector<ModelMaterial> m_materials;
    std::vector<ModelBone> m_bones;
    std::vector<SkinnedVertex> m_skinnedVertices;
};
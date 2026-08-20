#pragma once
#include "Vulkan/Vertex.h"
#include "Scene/Submesh.h"
#include <vector>
#include <string>

struct GltfLight
{
    int type = 0;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
};

struct GltfNavVolume
{
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 size{1.0f};
    float cellSize = 0.75f;
};

struct GltfImage
{
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
};

struct GltfDecal
{
    std::string texturePath;
    std::string opacityPath;
    std::string normalPath;
    std::string mrPath;
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 size{1.0f};
    float metallic = 0.0f;
    float roughness = 0.6f;
};

struct GltfMaterial
{
    int albedoImage = -1;
    int normalImage = -1;
    int mrImage = -1;
    int aoImage = -1;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    glm::vec4 baseColorFactor{1.0f};
    int lightmapImage = -1;
    bool hasLightmap = false;
};

struct GltfCollider
{
    std::string shape;
    std::string motion;
    float mass = 1.0f;
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 size{1.0f};
};

struct GltfEntity
{
    std::string name;
    std::string entityType;
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 scale{1.0f};
};

class GltfModel
{
public:
    void load(const std::string& path);

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<Submesh>& getSubmesh() const { return m_submesh; }
    const std::vector<GltfImage>& getImage() const { return m_images; }
    const std::vector<GltfMaterial>& getMaterials() const { return m_materials; } 
    const std::vector<GltfLight>& getLights() const { return m_lights; }
    const std::vector<GltfDecal>& getDecals() const { return m_decals; }
    const std::vector<GltfCollider>& getColliders() const { return m_colliders; }
    const std::vector<GltfEntity>& getEntities() const { return m_entities; }
    const std::vector<GltfNavVolume>& getNavVolumes() const { return m_navVolumes; }

private:
    void computeTangents();

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Submesh> m_submesh;
    std::vector<GltfImage> m_images;
    std::vector<GltfMaterial> m_materials;
    std::vector<GltfLight> m_lights;
    std::vector<GltfDecal> m_decals;
    std::vector<GltfCollider> m_colliders;
    std::vector<GltfEntity> m_entities;  
    std::vector<GltfNavVolume> m_navVolumes;
    bool m_hasTangents = false;
};
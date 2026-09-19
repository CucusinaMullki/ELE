#pragma once
#include "Vulkan/Vertex.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct LevelSubmesh
{
    int32_t materialIndex;
    uint32_t indexOffset;
    uint32_t indexCount;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
};

struct LevelMesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<LevelSubmesh> submeshes;
};

struct LevelMaterial
{
    std::string albedoPath;
    std::string normalPath;
    std::string metallicRoughnessPath;
    std::string aoPath;
};

struct LevelObject
{
    std::string name;
    int32_t meshIndex = -1;
    glm::mat4 transform{1.0f};
    std::unordered_map<std::string, std::string> customProperties;
};

struct LevelLight
{
    int32_t type;
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
};

class LevelLoader
{
public:
    void load(const std::string& path);

    const std::vector<LevelMesh>& getMeshes() const { return m_meshes; }
    const std::vector<LevelMaterial>& getMaterials() const { return m_materials; }
    const std::vector<LevelLight>& getLights() const { return m_lights; }
    const std::vector<LevelObject>& getObjects() const { return m_objects; }

private:
    void parseMeshes(const uint8_t* data);
    void parseMaterials(const uint8_t* data);
    void parseObjects(const uint8_t* data);
    void parseLights(const uint8_t* data);

    std::vector<LevelMesh> m_meshes;
    std::vector<LevelMaterial> m_materials;
    std::vector<LevelObject> m_objects;
    std::vector<LevelLight> m_lights;
};

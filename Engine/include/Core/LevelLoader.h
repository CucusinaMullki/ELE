#pragma once
#include "Vulkan/Vertex.h"
#include "Scene/Transform.h"
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
    Transform trs;
    bool hasTrs = false;
    std::unordered_map<std::string, std::string> customProperties;
};

struct LevelLight
{
    int32_t type = 0;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 0.0f;

    bool hasComponent = false;
    std::string name;
    Transform trs;
    std::vector<uint8_t> componentData;
};

struct LevelSpawn
{
    std::string name;
    std::string type;
    Transform trs;
    std::unordered_map<std::string, std::string> properties;
};

class LevelLoader
{
public:
    void load(const std::string& path);

    const std::vector<LevelMesh>& getMeshes() const { return m_meshes; }
    const std::vector<LevelMaterial>& getMaterials() const { return m_materials; }
    const std::vector<LevelLight>& getLights() const { return m_lights; }
    const std::vector<LevelObject>& getObjects() const { return m_objects; }

    const std::vector<LevelSpawn>& getSpawns() const { return m_spawns; }
    uint32_t getVersion() const { return m_version; }

private:
    void parseMeshes(const uint8_t* data);
    void parseMaterials(const uint8_t* data);
    void parseObjects(const uint8_t* data);
    void parseLights(const uint8_t* data);
    void parseObjectsV2(const uint8_t* data, size_t size);
    void parseLightsV2(const uint8_t* data, size_t size);
    void parseSpawnsV2(const uint8_t* data, size_t size);

    std::vector<LevelMesh> m_meshes;
    std::vector<LevelMaterial> m_materials;
    std::vector<LevelObject> m_objects;
    std::vector<LevelLight> m_lights;
    std::vector<LevelSpawn> m_spawns;
    uint32_t m_version = 0;
};

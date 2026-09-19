#include "Core/LevelLoader.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <thread>

namespace
{
    template<typename T>
    T readValue(const uint8_t* data, size_t& offset)
    {
        T value{};
        std::memcpy(&value, data + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }

    std::string readString(const uint8_t* data, size_t& offset)
    {
        uint32_t length = readValue<uint32_t>(data, offset);
        std::string result(reinterpret_cast<const char*>(data + offset), length);
        offset += length;
        return result;
    }
}

void LevelLoader::parseMeshes(const uint8_t* data)
{
    size_t offset = 0;
    uint32_t meshCount = readValue<uint32_t>(data, offset);
    m_meshes.resize(meshCount);

    for (auto& mesh : m_meshes)
    {
        uint32_t vertexCount = readValue<uint32_t>(data, offset);
        uint32_t indexCount = readValue<uint32_t>(data, offset);

        LOG("LevelLoader: vertexCount=" << vertexCount << " indexCount=" << indexCount);

        mesh.vertices.resize(vertexCount);
        std::memcpy(mesh.vertices.data(), data + offset, vertexCount * sizeof(Vertex));
        offset += vertexCount * sizeof(Vertex);

        if (vertexCount == 55464)
        {
            LOG("LevelLoader: first 3 vertices UV: "
                << "(" << mesh.vertices[0].texCoord.x << "," << mesh.vertices[0].texCoord.y << ") "
                << "(" << mesh.vertices[1].texCoord.x << "," << mesh.vertices[1].texCoord.y << ") "
                << "(" << mesh.vertices[2].texCoord.x << "," << mesh.vertices[2].texCoord.y << ")");
        }

        mesh.indices.resize(indexCount);
        std::memcpy(mesh.indices.data(), data + offset, indexCount * sizeof(uint32_t));
        offset += indexCount * sizeof(uint32_t);

        uint32_t submeshCount = readValue<uint32_t>(data, offset);
        LOG("LevelLoader: submeshCount=" << submeshCount);
        mesh.submeshes.resize(submeshCount);
        for (auto& sm : mesh.submeshes)
        {
            sm.materialIndex = readValue<int32_t>(data, offset);
            sm.indexOffset = readValue<int32_t>(data, offset);
            sm.indexCount = readValue<int32_t>(data, offset);
            sm.boundsMin = readValue<glm::vec3>(data, offset);
            sm.boundsMax = readValue<glm::vec3>(data, offset);
        }
    }
}

void LevelLoader::parseMaterials(const uint8_t* data)
{
    size_t offset = 0;
    uint32_t materialCount = readValue<uint32_t>(data, offset);
    LOG("LevelLoader: materialCount=" << materialCount);
    m_materials.resize(materialCount);

    for (auto& mat : m_materials)
    {
        mat.albedoPath = readString(data, offset);
        mat.normalPath = readString(data, offset);
        mat.metallicRoughnessPath = readString(data, offset);
        mat.aoPath = readString(data, offset);
    }
}

void LevelLoader::parseObjects(const uint8_t* data)
{
    size_t offset = 0;
    uint32_t objectCount = readValue<uint32_t>(data, offset);
    LOG("LevelLoader: objectCount=" << objectCount);
    m_objects.resize(objectCount);

    for (auto& obj : m_objects)
    {
        obj.name = readString(data, offset);
        obj.meshIndex = readValue<int32_t>(data, offset);
        obj.transform = readValue<glm::mat4>(data, offset);

        uint32_t propCount = readValue<uint32_t>(data, offset);
        for (uint32_t p = 0; p < propCount; p++)
        {
            std::string key = readString(data, offset);
            std::string value = readString(data, offset);
            obj.customProperties[key] = value;
        }
    }
}

void LevelLoader::parseLights(const uint8_t* data)
{
    size_t offset = 0;
    uint32_t lightCount = readValue<uint32_t>(data, offset);
    m_lights.resize(lightCount);

    for (auto& light : m_lights)
    {
        light.type = readValue<int32_t>(data, offset);
        light.position = readValue<glm::vec3>(data, offset);
        light.direction = readValue<glm::vec3>(data, offset);
        light.color = readValue<glm::vec3>(data, offset);
        light.intensity = readValue<float>(data, offset);
    }
}

void LevelLoader::load(const std::string& path)
{
    std::string fullPath = (Paths::assetDir() / path).string();

    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("failed to open level file: " + fullPath);

    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    const uint8_t* data = buffer.data();

    char magic[5] = {0};
    std::memcpy(magic, data, 4);
    if (std::strcmp(magic, "ELVL") != 0)
        throw std::runtime_error("invalid level file magic: " + fullPath);

    size_t offset = 4;
    uint32_t version = readValue<uint32_t>(data, offset);
    (void)version;

    uint64_t meshesOffset = readValue<uint64_t>(data, offset);
    uint64_t materialsOffset = readValue<uint64_t>(data, offset);
    uint64_t objectsOffset = readValue<uint64_t>(data, offset);
    uint64_t lightsOffset = readValue<uint64_t>(data, offset);

    LOG("LevelLoader: fileSize=" << buffer.size() << " meshesOffset=" << meshesOffset
        << " materialsOffset=" << materialsOffset << " objectsOffset=" << objectsOffset
        << " lightsOffset=" << lightsOffset);

    std::thread meshThread([&]() { parseMeshes(data + meshesOffset); });
    std::thread materialThread([&]() { parseMaterials(data + materialsOffset); });
    std::thread objectThread([&]() { parseObjects(data + objectsOffset); });
    std::thread lightThread([&]() { parseLights(data + lightsOffset); });

    meshThread.join();
    materialThread.join();
    objectThread.join();
    lightThread.join();
}
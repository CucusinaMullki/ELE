#include "Core/LevelLoader.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include "Core/BinaryStream.h"
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

        mesh.vertices.resize(vertexCount);
        std::memcpy(mesh.vertices.data(), data + offset, vertexCount * sizeof(Vertex));
        offset += vertexCount * sizeof(Vertex);

        mesh.indices.resize(indexCount);
        std::memcpy(mesh.indices.data(), data + offset, indexCount * sizeof(uint32_t));
        offset += indexCount * sizeof(uint32_t);

        uint32_t submeshCount = readValue<uint32_t>(data, offset);
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

namespace
{
    Transform readTrs(BinaryReader& r)
    {
        Transform t;
        t.position = r.read<glm::vec3>();
        t.rotation = r.read<glm::vec3>();
        t.scale    = r.read<glm::vec3>();
        return t;
    }

    void readProps(BinaryReader& r, std::unordered_map<std::string, std::string>& out)
    {
        const uint32_t n = r.read<uint32_t>();
        for (uint32_t i = 0; i < n && r.ok(); ++i)
        {
            std::string k = r.readString();
            out[k] = r.readString();
        }
    }
}

void LevelLoader::parseObjectsV2(const uint8_t* data, size_t size)
{
    BinaryReader r(data, size);
    const uint32_t n = r.read<uint32_t>();
    for (uint32_t i = 0; i < n && r.ok(); ++i)
    {
        LevelObject o;
        o.name      = r.readString();
        o.meshIndex = r.read<int32_t>();
        o.trs       = readTrs(r);
        o.hasTrs    = true;
        o.transform = o.trs.getMatrix();
        readProps(r, o.customProperties);
        if (r.ok()) m_objects.push_back(std::move(o));
    }
}

void LevelLoader::parseLightsV2(const uint8_t* data, size_t size)
{
    BinaryReader r(data, size);
    const uint32_t n = r.read<uint32_t>();
    for (uint32_t i = 0; i < n && r.ok(); ++i)
    {
        LevelLight l;
        l.hasComponent = true;
        l.name = r.readString();
        l.trs  = readTrs(r);
        BinaryReader block = r.readBlock();
        l.componentData.assign(block.cursor(), block.cursor() + block.remaining());
        l.position = l.trs.position;
        if (r.ok()) m_lights.push_back(std::move(l));
    }
}

void LevelLoader::parseSpawnsV2(const uint8_t* data, size_t size)
{
    BinaryReader r(data, size);
    const uint32_t n = r.read<uint32_t>();
    for (uint32_t i = 0; i < n && r.ok(); ++i)
    {
        LevelSpawn sp;
        sp.name = r.readString();
        sp.type = r.readString();
        sp.trs  = readTrs(r);
        readProps(r, sp.properties);
        if (r.ok()) m_spawns.push_back(std::move(sp));
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

    if (fileSize < 40)
        throw std::runtime_error("level file too small: " + fullPath);

    const uint8_t* data = buffer.data();

    char magic[5] = {0};
    std::memcpy(magic, data, 4);
    if (std::strcmp(magic, "ELVL") != 0)
        throw std::runtime_error("invalid level file magic: " + fullPath);

    size_t offset = 4;
    m_version = readValue<uint32_t>(data, offset);

    uint64_t meshesOffset = readValue<uint64_t>(data, offset);
    uint64_t materialsOffset = readValue<uint64_t>(data, offset);
    uint64_t objectsOffset = readValue<uint64_t>(data, offset);
    uint64_t lightsOffset = readValue<uint64_t>(data, offset);

    m_meshes.clear(); m_materials.clear(); m_objects.clear(); m_lights.clear(); m_spawns.clear();

    const bool v2 = m_version >= 2;
    uint64_t spawnsOffset = 0;
    if (v2)
    {
        if (fileSize < 48)
            throw std::runtime_error("level v2 header truncated: " + fullPath);
        spawnsOffset = readValue<uint64_t>(data, offset);
    }
    for (uint64_t o : { meshesOffset, materialsOffset, objectsOffset, lightsOffset, spawnsOffset })
        if (o >= fileSize)
            throw std::runtime_error("level section out of file: " + fullPath);

    auto rest = [&](uint64_t o) { return (size_t)(fileSize - o); };

    std::thread meshThread([&]() { parseMeshes(data + meshesOffset); });
    std::thread materialThread([&]() { parseMaterials(data + materialsOffset); });
    std::thread objectThread([&]() {
        if (v2) parseObjectsV2(data + objectsOffset, rest(objectsOffset));
        else parseObjects(data + objectsOffset);
    });
    std::thread lightThread([&]() {
        if (v2) { parseLightsV2(data + lightsOffset, rest(lightsOffset));
            parseSpawnsV2(data + spawnsOffset, rest(spawnsOffset)); }
        else    parseLights(data + lightsOffset);
    });

    meshThread.join();
    materialThread.join();
    objectThread.join();
    lightThread.join();

    LOG("LevelLoader: v" << m_version << " meshes=" << m_meshes.size() << " objects=" << m_objects.size()
        << " lights=" << m_lights.size() << " spawns=" << m_spawns.size());
}

#include "Core/ModelLoader.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include <meshoptimizer.h>
#include <fstream>
#include <stdexcept>
#include <cstring>

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

void ModelLoader::load(const std::string& path)
{
    std::string fullPath = (Paths::assetDir() / path).string();

    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("failed to open model file: " + fullPath);

    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    const uint8_t* data = buffer.data();
    size_t offset = 0;

    char magic[5] = {0};
    std::memcpy(magic, data, 4);
    offset += 4;
    if (std::strcmp(magic, "EMDL") != 0)
        throw std::runtime_error("invalid model file magic: " + fullPath);

    uint32_t version = readValue<uint32_t>(data, offset);
    uint32_t flags = readValue<uint32_t>(data, offset);
    (void)version;
    bool hasSkeleton = (flags & 1) != 0;

    if (hasSkeleton)
    {
        uint32_t boneCount = readValue<uint32_t>(data, offset);
        m_bones.resize(boneCount);
        for (auto& bone : m_bones)
        {
            bone.name = readString(data, offset);
            bone.parentIndex = readValue<int32_t>(data, offset);
            bone.inverseBindMatrix = readValue<glm::mat4>(data, offset);
        }
    }

    uint32_t vertexStride = readValue<uint32_t>(data, offset);
    uint32_t vertexCount = readValue<uint32_t>(data, offset);
    uint32_t compressedVertexSize = readValue<uint32_t>(data, offset);

    std::vector<uint8_t> decodedVertices(vertexCount * vertexStride);
    int rc = meshopt_decodeVertexBuffer(decodedVertices.data(), vertexCount, vertexStride,
        data + offset, compressedVertexSize);
    if (rc != 0)
        throw std::runtime_error("meshopt_decodeVertexBuffer failed for: " + fullPath);
    offset += compressedVertexSize;

    if (hasSkeleton)
    {
        m_skinnedVertices.resize(vertexCount);
        std::memcpy(m_skinnedVertices.data(), decodedVertices.data(), decodedVertices.size());
    }
    else
    {
        m_vertices.resize(vertexCount);
        std::memcpy(m_vertices.data(), decodedVertices.data(), decodedVertices.size());
    }

    uint32_t indexCount = readValue<uint32_t>(data, offset);
    uint32_t compressedIndexSize = readValue<uint32_t>(data, offset);

    m_indices.resize(indexCount);
    rc = meshopt_decodeIndexBuffer(m_indices.data(), indexCount, sizeof(uint32_t),
        data + offset, compressedIndexSize);
    if (rc != 0)
        throw std::runtime_error("meshopt_decodeIndexBuffer failed for: " + fullPath);
    offset += compressedIndexSize;

    LOG("ModelLoader: offset before submeshCount=" << offset);
    uint32_t submeshCount = readValue<uint32_t>(data, offset);
    LOG("ModelLoader: submeshCount=" << submeshCount);
    m_submeshes.resize(submeshCount);
    for (auto& sm : m_submeshes)
    {
        sm.materialIndex = readValue<int32_t>(data, offset);
        sm.indexOffset = readValue<uint32_t>(data, offset);
        sm.indexCount = readValue<uint32_t>(data, offset);
        sm.boundsMin = readValue<glm::vec3>(data, offset);
        sm.boundsMax = readValue<glm::vec3>(data, offset);
    }

    LOG("ModelLoader: offset before materialCount read=" << offset << " bufferSize=" << buffer.size());
    uint32_t materialCount = readValue<uint32_t>(data, offset);
    LOG("ModelLoader: materialCount=" << materialCount);
    m_materials.resize(materialCount);
    for (auto& mat : m_materials)
    {
        mat.albedoPath = readString(data, offset);
        mat.normalPath = readString(data, offset);
        mat.metallicRoughnessPath = readString(data, offset);
        mat.aoPath = readString(data, offset);
    }
}
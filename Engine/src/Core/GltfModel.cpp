#include "Core/GltfModel.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include <tiny_gltf.h>
#include <stdexcept>
#include <cstring>

namespace 
{
    const unsigned char* getAccessorPtr(const tinygltf::Model& model, int accessorIdx, size_t& count, size_t& stride)
    {
        const auto& accessor = model.accessors[accessorIdx];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];
        
        count = accessor.count;
        stride = accessor.ByteStride(bufferView);

        return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    }
}

void GltfModel::load(const std::string& path)
{
    std::string fullpath = (Paths::assetDir() / path).string();

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool isBinary = fullpath.size() > 4 && fullpath.substr(fullpath.size() - 4) == ".glb";

    bool ok = isBinary ? loader.LoadBinaryFromFile(&model, &err, &warn, fullpath) : loader.LoadASCIIFromFile(&model, &err, &warn, fullpath);
    
    if (!warn.empty()) LOG("gltf warning: " << warn);
    if (!ok) throw std::runtime_error("failed to load glTF: " + fullpath + " " + err);

    for (const auto& mesh : model.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.indices < 0) continue;

            uint32_t vertexStart = static_cast<uint32_t>(m_vertices.size());

            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) continue;

            size_t vertexCount = 0, posStride = 0;
            const unsigned char* posData = getAccessorPtr(model, posIt->second, vertexCount, posStride);

            const unsigned char* normData = nullptr;
            size_t normStride = 0, dummy = 0;
            auto normIt = primitive.attributes.find("NORMAL");
            if (normIt != primitive.attributes.end())
                normData = getAccessorPtr(model, normIt->second, dummy, normStride);
            
            const unsigned char* uvData = nullptr;
            size_t uvStride = 0;
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end())
                uvData = getAccessorPtr(model, uvIt->second, dummy, uvStride);

            const unsigned char* tanData = nullptr;
            size_t tanStride = 0;
            auto tanIt = primitive.attributes.find("TANGENT");
            if (tanIt != primitive.attributes.end())
            {
                tanData = getAccessorPtr(model, tanIt->second, dummy, tanStride);
                m_hasTangents = true;
            }

            for (size_t i = 0; i < vertexCount; i++)
            {
                Vertex v{};

                std::memcpy(&v.pos, posData + i * posStride, sizeof(glm::vec3));

                if (normData)
                    std::memcpy(&v.normal, normData + i * normStride, sizeof(glm::vec3));
                else
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);

                if (uvData)
                    std::memcpy(&v.texCoord, uvData + i * uvStride, sizeof(glm::vec2));
                else
                    v.texCoord = glm::vec2(0.0f);

                if (tanData)
                {
                    glm::vec4 t{};
                    std::memcpy(&t, tanData + i * tanStride, sizeof(glm::vec4));
                    v.tangent = glm::vec3(t);
                }
                else
                {
                    v.tangent = glm::vec3(0.0f);
                }

                v.color = glm::vec3(1.0f);
                m_vertices.push_back(v);
            }

            Submesh sm{};
            sm.firstIndex = static_cast<uint32_t>(m_indices.size());
            sm.materialIndex = primitive.material >= 0 ? primitive.material : 0;

            const auto& idxAccessor = model.accessors[primitive.indices];
            size_t idxCount = 0, idxStride = 0;
            const unsigned char* idxData = getAccessorPtr(model, primitive.indices, idxCount, idxStride);

            for (size_t i = 0; i < idxCount; i++)
            {
                uint32_t index = 0;
                const unsigned char* p = idxData + i * idxStride;

                switch (idxAccessor.componentType)
                {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        index = *reinterpret_cast<const uint32_t*>(p);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        index = *reinterpret_cast<const uint16_t*>(p);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        index = *p;
                        break;
                    default:
                        throw std::runtime_error("unsupported glTF index type");
                }

                m_indices.push_back(vertexStart + index);
            }
        
            sm.indexCount = static_cast<uint32_t>(idxCount);
            m_submesh.push_back(sm);
        }
    }

    for (const auto& img : model.images)
    {
        GltfImage gi{};
        gi.width = img.width;
        gi.height = img.height;
        
        if (img.component == 4)
        {
            gi.pixels = img.image;
        }
        else if (img.component == 3)
        {
            gi.pixels.resize(img.width * img.height * 4);
            for (int i = 0; i < img.width * img.height; i++)
            {
                gi.pixels[i*4+0] = img.image[i*3+0];
                gi.pixels[i*4+1] = img.image[i*3+1];
                gi.pixels[i*4+2] = img.image[i*3+2];
                gi.pixels[i*4+3] = 255;
            }
        }
        else
        {
            gi.width = 1; 
            gi.height = 1;
            gi.pixels = {255, 255, 255, 255};
        }

        m_images.push_back(gi);
    }

    for (const auto& mat : model.materials)
    {
        GltfMaterial gm{};

        const auto& pbr = mat.pbrMetallicRoughness;

        if (pbr.baseColorTexture.index >= 0)
            gm.albedoImage = model.textures[pbr.baseColorTexture.index].source;

        if (pbr.metallicRoughnessTexture.index >= 0)
            gm.mrImage = model.textures[pbr.metallicRoughnessTexture.index].source;

        if (mat.normalTexture.index >= 0)
            gm.normalImage = model.textures[mat.normalTexture.index].source;

        if (mat.occlusionTexture.index >= 0)
            gm.aoImage = model.textures[mat.occlusionTexture.index].source;

        m_materials.push_back(gm);
    }

    if (!m_hasTangents)
        computeTangents();

    LOG("glTF loaded: " << m_vertices.size() << " verts, " << m_indices.size() << " indices, " << m_submesh.size() << " submeshes");
}

void GltfModel::computeTangents()
{
    for (auto& v : m_vertices)
        v.tangent = glm::vec3(0.0f);

    for (size_t i = 0; i + 2 < m_indices.size(); i += 3)
    {
        Vertex& v0 = m_vertices[m_indices[i + 0]];
        Vertex& v1 = m_vertices[m_indices[i + 1]];
        Vertex& v2 = m_vertices[m_indices[i + 2]];

        glm::vec3 edge1 = v1.pos - v0.pos;
        glm::vec3 edge2 = v2.pos - v0.pos;
        glm::vec2 dUV1 = v1.texCoord - v0.texCoord;
        glm::vec2 dUV2 = v2.texCoord - v0.texCoord;

        float denom = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
        float f = (denom == 0.0f) ? 0.0f : 1.0f / denom;

        glm::vec3 tangent;
        tangent.x = f * (dUV2.y * edge1.x - dUV1.y * edge2.x);
        tangent.y = f * (dUV2.y * edge1.y - dUV1.y * edge2.y);
        tangent.z = f * (dUV2.y * edge1.z - dUV1.y * edge2.z);

        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;        
    }

    for (auto& v : m_vertices)
    {
        if (glm::length(v.tangent) > 0.0001f)
            v.tangent = glm::normalize(v.tangent);
        else
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    }
}
#include "Core/GltfModel.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_gltf.h>
#include <unordered_set>
#include <stdexcept>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

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

    std::unordered_set<int> collisionOnlyMeshIndices;
    for (const auto& node : model.nodes)
    {
        if (node.extras.Has("collision_shape") && node.mesh >= 0)
            collisionOnlyMeshIndices.insert(node.mesh);
    }    

    for (const auto& mesh : model.meshes)
    {
        int meshIndex = static_cast<int>(&mesh - &model.meshes[0]);;
        if (collisionOnlyMeshIndices.count((int)meshIndex))
            continue;
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

            const unsigned char* uv2Data = nullptr;
            size_t uv2Stride = 0;
            auto uv2It = primitive.attributes.find("TEXCOORD_1");
            if (uv2It != primitive.attributes.end())
                uv2Data = getAccessorPtr(model, uv2It->second, dummy, uv2Stride);

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

                if (uv2Data)
                    std::memcpy(&v.lightmapUV, uv2Data + i * uv2Stride, sizeof(glm::vec2));
                else
                    v.lightmapUV = v.texCoord;

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

        if (pbr.baseColorFactor.size() == 4)
            gm.baseColorFactor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                pbr.baseColorFactor[2], pbr.baseColorFactor[3]);

        if (pbr.metallicRoughnessTexture.index >= 0)
            gm.mrImage = model.textures[pbr.metallicRoughnessTexture.index].source;

        if (mat.normalTexture.index >= 0)
            gm.normalImage = model.textures[mat.normalTexture.index].source;

        if (mat.occlusionTexture.index >= 0)
            gm.aoImage = model.textures[mat.occlusionTexture.index].source;

        LOG("gltf material: lightmapImage=" << gm.lightmapImage << " hasLightmap=" << gm.hasLightmap);
        if (mat.emissiveTexture.index >= 0)
        {
            gm.lightmapImage = model.textures[mat.emissiveTexture.index].source;
            gm.hasLightmap = true;
        }

        gm.metallicFactor = (float)pbr.metallicFactor;
        gm.roughnessFactor = (float)pbr.roughnessFactor;

        LOG("gltf material: mrImage=" << gm.mrImage
            << " metallicFactor=" << gm.metallicFactor
            << " roughnessFactor=" << gm.roughnessFactor);

        m_materials.push_back(gm);
    }

    for (const auto& node : model.nodes)
    {

        if (node.extras.Has("collision_shape"))
            LOG("found collision_shape on node, node.mesh=" << node.mesh);

        if (node.extras.Has("collision_shape") && node.mesh >= 0)
            collisionOnlyMeshIndices.insert(node.mesh);

        if (node.extras.Has("collision_shape"))
        {
            GltfCollider collider{};
            collider.shape = node.extras.Get("collision_shape").Get<std::string>();
            collider.motion = node.extras.Has("collision_motion")
                ? node.extras.Get("collision_motion").Get<std::string>() : "static";
            collider.mass = node.extras.Has("collision_mass")
                ? (float)node.extras.Get("collision_mass").Get<double>() : 1.0f;
        
            glm::vec3 nodeScale(1.0f);
            if (!node.matrix.empty() && node.matrix.size() == 16)
            {
                glm::mat4 m = glm::make_mat4(node.matrix.data());
                glm::vec3 skew; glm::vec4 persp; glm::quat rotQ;
                glm::decompose(m, nodeScale, rotQ, collider.position, skew, persp);
                collider.rotationEuler = glm::degrees(glm::eulerAngles(rotQ));
            }
            else
            {
                if (node.translation.size() >= 3)
                    collider.position = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
                if (node.rotation.size() >= 4)
                {
                    glm::quat q((float)node.rotation[3], (float)node.rotation[0],
                                (float)node.rotation[1], (float)node.rotation[2]);
                    collider.rotationEuler = glm::degrees(glm::eulerAngles(q));
                }
                if (node.scale.size() >= 3)
                    nodeScale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }
        
            if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
            {
                const auto& mesh = model.meshes[node.mesh];
                if (!mesh.primitives.empty())
                {
                    auto posIt = mesh.primitives[0].attributes.find("POSITION");
                    if (posIt != mesh.primitives[0].attributes.end())
                    {
                        const auto& accessor = model.accessors[posIt->second];
                        if (accessor.minValues.size() == 3 && accessor.maxValues.size() == 3)
                        {
                            glm::vec3 localMin((float)accessor.minValues[0], (float)accessor.minValues[1], (float)accessor.minValues[2]);
                            glm::vec3 localMax((float)accessor.maxValues[0], (float)accessor.maxValues[1], (float)accessor.maxValues[2]);
                            glm::vec3 halfExtent = (localMax - localMin) * 0.5f;
                            collider.size = halfExtent * nodeScale;
                        }
                    }
                }
            }
            else
            {
                collider.size = nodeScale;
            }
        
            m_colliders.push_back(collider);
            continue;
        }
        if (node.extras.Has("decal_texture"))
        {
            GltfDecal decal{};
            decal.texturePath = node.extras.Get("decal_texture").Get<std::string>();
            LOG("Texture was imported");

            if (node.extras.Has("decal_metallic"))
                decal.metallic = (float)node.extras.Get("decal_metallic").Get<double>();
            if (node.extras.Has("decal_roughness"))
                decal.roughness = (float)node.extras.Get("decal_roughness").Get<double>();
            if (node.extras.Has("decal_opacity"))
                decal.opacityPath = node.extras.Get("decal_opacity").Get<std::string>(); LOG("Texture was imported");
            if (node.extras.Has("decal_normal"))
                decal.normalPath = node.extras.Get("decal_normal").Get<std::string>();
            if (node.extras.Has("decal_mr"))
                decal.mrPath = node.extras.Get("decal_mr").Get<std::string>();
                
            if (!node.matrix.empty() && node.matrix.size() == 16)
            {
                glm::mat4 m = glm::make_mat4(node.matrix.data());
                glm::vec3 skew; glm::vec4 persp; glm::quat rotQ;
                glm::decompose(m, decal.size, rotQ, decal.position, skew, persp);
                decal.rotationEuler = glm::degrees(glm::eulerAngles(rotQ));
            }
            else
            {
                if (node.translation.size() >= 3)
                    decal.position = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);

                if (node.rotation.size() >= 4)
                {
                    glm::quat q((float)node.rotation[3], (float)node.rotation[0],
                        (float)node.rotation[1], (float)node.rotation[2]);
                    decal.rotationEuler = glm::degrees(glm::eulerAngles(q));
                }

                if (node.scale.size() >= 3)
                    decal.size = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }

            LOG("gltf decal: tex=" << decal.texturePath
                << " pos=(" << decal.position.x << ", " << decal.position.y << ", " << decal.position.z << ")");

            m_decals.push_back(decal);
            continue;
        
        }

        if (node.light < 0) continue;
        if (node.light >= (int)model.lights.size()) continue;

        const auto& gltfLight = model.lights[node.light];

        GltfLight light{};

        // Тип
        if (gltfLight.type == "directional") light.type = 0;
        else if (gltfLight.type == "point")  light.type = 1;
        else if (gltfLight.type == "spot")   light.type = 2;

        if (gltfLight.color.size() >= 3)
            light.color = glm::vec3(gltfLight.color[0], gltfLight.color[1], gltfLight.color[2]);

        light.intensity = (float)gltfLight.intensity;

        if (!node.matrix.empty() && node.matrix.size() == 16)
        {
            glm::mat4 m = glm::make_mat4(node.matrix.data());
            light.position = glm::vec3(m[3]);
            light.direction = glm::normalize(glm::vec3(m * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
        }
        else
        {
            if (node.translation.size() >= 3)
                light.position = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);

            if (node.rotation.size() >= 4)
            {
                glm::quat q(
                    (float)node.rotation[3],
                    (float)node.rotation[0],
                    (float)node.rotation[1],
                    (float)node.rotation[2]
                );
                light.direction = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
            }
        }

        

        LOG("gltf light: type=" << light.type
            << " intensity=" << light.intensity
            << " pos=(" << light.position.x << "," << light.position.y << "," << light.position.z << ")"
            << " dir=(" << light.direction.x << "," << light.direction.y << "," << light.direction.z << ")");

        m_lights.push_back(light);
    }

    LOG("extracted lights: " << m_lights.size());

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
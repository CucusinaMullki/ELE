#include "Core/Model.h"
#include "Core/Path.h"
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <stdexcept>
#include <glm/glm.hpp>

void Model::load(const std::string& path)
{
    std::string fullPath = (Paths::assetDir() / path).string();
    std::string baseDir  = (Paths::assetDir() / path).parent_path().string();

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        fullPath.c_str(), baseDir.c_str(), true))
    {
        throw std::runtime_error("failed to load model: " + fullPath + " " + warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0)
            {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            else
            {
                vertex.normal = { 0.0f, 1.0f, 0.0f };
            }

            if (index.texcoord_index >= 0)
            {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else
            {
                vertex.texCoord = { 0.0f, 0.0f };
            }

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                m_vertices.push_back(vertex);
            }

            m_indices.push_back(uniqueVertices[vertex]);
        }
    }

    for (auto& v : m_vertices)
        v.tangent = glm::vec3(0.0f);

    for (size_t i = 0; i + 2 < m_indices.size(); i += 3)
    {
        Vertex& v0 = m_vertices[m_indices[i + 0]];
        Vertex& v1 = m_vertices[m_indices[i + 1]];
        Vertex& v2 = m_vertices[m_indices[i + 2]];

        glm::vec3 edge1 = v1.pos - v0.pos;
        glm::vec3 edge2 = v2.pos - v0.pos;
        glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
        glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

        float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        float f = (denom == 0.0f) ? 0.0f : 1.0f / denom;

        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

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
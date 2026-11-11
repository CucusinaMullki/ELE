#pragma once
#include "ASH/ASH.h"
#include <glm/glm.hpp>
#include <array>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 lightmapUV;

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color &&
            texCoord == other.texCoord && normal == other.normal;
    }

    static ASH::VertexBindingDesc getBindingDescriptorASH()
    {
        ASH::VertexBindingDesc binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.perInstance = false;
        return binding;
    }

    static std::array<ASH::VertexAttributeDesc, 6> getAttributeDescriptionsASH()
    {
        std::array<ASH::VertexAttributeDesc, 6> attributes{};

        attributes[0].binding = 0; attributes[0].location = 0;
        attributes[0].format = ASH::Format::R32G32B32_Float;
        attributes[0].offset = offsetof(Vertex, pos);

        attributes[1].binding = 0; attributes[1].location = 1;
        attributes[1].format = ASH::Format::R32G32B32_Float;
        attributes[1].offset = offsetof(Vertex, color);

        attributes[2].binding = 0; attributes[2].location = 2;
        attributes[2].format = ASH::Format::R32G32_Float;
        attributes[2].offset = offsetof(Vertex, texCoord);

        attributes[3].binding = 0; attributes[3].location = 3;
        attributes[3].format = ASH::Format::R32G32B32_Float;
        attributes[3].offset = offsetof(Vertex, normal);

        attributes[4].binding = 0; attributes[4].location = 4;
        attributes[4].format = ASH::Format::R32G32B32_Float;
        attributes[4].offset = offsetof(Vertex, tangent);

        attributes[5].binding = 0; attributes[5].location = 5;
        attributes[5].format = ASH::Format::R32G32_Float;
        attributes[5].offset = offsetof(Vertex, lightmapUV);

        return attributes;
    }
};

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(const Vertex& v) const
        {
            size_t h1 = hash<float>()(v.pos.x) ^ (hash<float>()(v.pos.y) << 1) ^ (hash<float>()(v.pos.z) << 2);
            size_t h2 = hash<float>()(v.texCoord.x) ^ (hash<float>()(v.texCoord.y) << 1);
            size_t h3 = hash<float>()(v.normal.x) ^ (hash<float>()(v.normal.z) << 1);
            return h1 ^ (h2 << 3) ^ (h3 << 6);
        }
    };
}

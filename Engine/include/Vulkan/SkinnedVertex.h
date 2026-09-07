#pragma once
#include "ASH/ASH.h"
#include <glm/glm.hpp>
#include <array>

struct SkinnedVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
    glm::vec4 boneIndices{0.0f};
    glm::vec4 boneWeights{0.0f};

    static ASH::VertexBindingDesc getBindingDescriptorASH()
    {
        ASH::VertexBindingDesc binding{};
        binding.binding = 0;
        binding.stride = sizeof(SkinnedVertex);
        binding.perInstance = false;
        return binding;
    }

    static std::array<ASH::VertexAttributeDesc, 6> getAttributeDescriptionsASH()
    {
        std::array<ASH::VertexAttributeDesc, 6> attributes{};

        attributes[0].binding = 0; attributes[0].location = 0;
        attributes[0].format = ASH::Format::R32G32B32_Float;
        attributes[0].offset = offsetof(SkinnedVertex, pos);

        attributes[1].binding = 0; attributes[1].location = 1;
        attributes[1].format = ASH::Format::R32G32B32_Float;
        attributes[1].offset = offsetof(SkinnedVertex, normal);

        attributes[2].binding = 0; attributes[2].location = 2;
        attributes[2].format = ASH::Format::R32G32_Float;
        attributes[2].offset = offsetof(SkinnedVertex, texCoord);

        attributes[3].binding = 0; attributes[3].location = 3;
        attributes[3].format = ASH::Format::R32G32B32_Float;
        attributes[3].offset = offsetof(SkinnedVertex, tangent);

        attributes[4].binding = 0; attributes[4].location = 4;
        attributes[4].format = ASH::Format::R32G32B32A32_Float;
        attributes[4].offset = offsetof(SkinnedVertex, boneIndices);

        attributes[5].binding = 0; attributes[5].location = 5;
        attributes[5].format = ASH::Format::R32G32B32A32_Float;
        attributes[5].offset = offsetof(SkinnedVertex, boneWeights);

        return attributes;
    }

};
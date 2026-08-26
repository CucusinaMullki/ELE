#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <array>

struct SkinnedVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
    glm::ivec4 boneIndices{0};
    glm::vec4 boneWeights{0.0f};

    static vk::VertexInputBindingDescription getBindingDescriptor()
    {
        vk::VertexInputBindingDescription binding{};
        binding.setBinding(0);
        binding.setStride(sizeof(SkinnedVertex));
        binding.setInputRate(vk::VertexInputRate::eVertex);
        return binding;
    }

    static std::array<vk::VertexInputAttributeDescription, 6> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 6> attributes;

        attributes[0].setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setOffset(offsetof(SkinnedVertex, pos));
        
        attributes[1].setBinding(0).setLocation(1).setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setOffset(offsetof(SkinnedVertex, normal));
        
        attributes[2].setBinding(0).setLocation(2).setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(SkinnedVertex, texCoord));
        
        attributes[3].setBinding(0).setLocation(3).setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setOffset(offsetof(SkinnedVertex, tangent));
        
        attributes[4].setBinding(0).setLocation(4).setFormat(vk::Format::eR32G32B32A32Sint)
            .setOffset(offsetof(SkinnedVertex, boneIndices));

        attributes[5].setBinding(0).setLocation(5).setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setOffset(offsetof(SkinnedVertex, boneWeights));
        
        return attributes;   
    }

};
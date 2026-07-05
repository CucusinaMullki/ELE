#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <array>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }

    static vk::VertexInputBindingDescription getBindingDescriptor()
    {
        vk::VertexInputBindingDescription binding{};
        binding.setBinding(0);
        binding.setStride(sizeof(Vertex));
        binding.setInputRate(vk::VertexInputRate::eVertex);
        return binding;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 3> attributes{};

        attributes[0].setBinding(0);
        attributes[0].setLocation(0);
        attributes[0].setFormat(vk::Format::eR32G32B32Sfloat);
        attributes[0].setOffset(offsetof(Vertex, pos));

        attributes[1].setBinding(0);
        attributes[1].setLocation(1);
        attributes[1].setFormat(vk::Format::eR32G32B32Sfloat);
        attributes[1].setOffset(offsetof(Vertex, color));

        attributes[2].setBinding(0);
        attributes[2].setLocation(2);
        attributes[2].setFormat(vk::Format::eR32G32Sfloat);
        attributes[2].setOffset(offsetof(Vertex, texCoord));

        return attributes;
    }
};

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(const Vertex& vertex) const
        {
            size_t h1 = hash<float>()(vertex.pos.x) ^ (hash<float>()(vertex.pos.y) << 1) ^ (hash<float>()(vertex.pos.z) << 2);
            size_t h2 = hash<float>()(vertex.texCoord.x) ^ (hash<float>()(vertex.texCoord.y) << 1);
            return h1 ^ (h2 << 1);
        }
    };
}
#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <array>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && 
            texCoord == other.texCoord && normal == other.normal;
    }

    static vk::VertexInputBindingDescription getBindingDescriptor()
    {
        vk::VertexInputBindingDescription binding{};
        binding.setBinding(0);
        binding.setStride(sizeof(Vertex));
        binding.setInputRate(vk::VertexInputRate::eVertex);
        return binding;
    }

    static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 5> attributes{};

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

        attributes[3].setBinding(0);
        attributes[3].setLocation(3);
        attributes[3].setFormat(vk::Format::eR32G32B32Sfloat);
        attributes[3].setOffset(offsetof(Vertex, normal));

        attributes[4].setBinding(0);
        attributes[4].setLocation(4);
        attributes[4].setFormat(vk::Format::eR32G32B32Sfloat);
        attributes[4].setOffset(offsetof(Vertex, tangent));

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
#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <array>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescriptor()
    {
        vk::VertexInputBindingDescription binding{};
        binding.setBinding(0);
        binding.setStride(sizeof(Vertex));
        binding.setInputRate(vk::VertexInputRate::eVertex);
        return binding;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attributes{};

        attributes[0].setBinding(0);
        attributes[0].setLocation(0);
        attributes[0].setFormat(vk::Format::eR32G32B32Sfloat);
        attributes[9].setOffset(offsetof(Vertex, pos));

        attributes[1].setBinding(0);
        attributes[1].setLocation(1);
        attributes[1].setFormat(vk::Format::eR32G32B32Sfloat);
        attributes[1].setOffset(offsetof(Vertex, color));

        return attributes;
    }
};
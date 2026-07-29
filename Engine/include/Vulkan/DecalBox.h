#pragma once
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/IndexBuffer.h"
#include <vulkan/vulkan.hpp>

class DecalBox
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice);
    void cleanup();

    void bind(vk::CommandBuffer cmd) const;
    void draw(vk::CommandBuffer cmd) const;

private:
    VertexBuffer m_vertexBuffer;
    IndexBuffer m_indexBuffer;
    uint32_t m_indexCount = 0;
};
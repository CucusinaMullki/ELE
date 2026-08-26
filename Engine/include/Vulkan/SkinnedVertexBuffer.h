#pragma once
#include <vulkan/vulkan.hpp>
#include "Vulkan/SkinnedVertex.h"
#include <vector>

class SkinnedVertexBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, const std::vector<SkinnedVertex>& vertices);
    void cleanup();
    vk::Buffer getBuffer() const { return m_buffer.get(); }
    uint32_t getVertexCount() const { return m_vertexCount; }

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    vk::UniqueBuffer m_buffer;
    vk::UniqueDeviceMemory m_memory;
    uint32_t m_vertexCount = 0;
};
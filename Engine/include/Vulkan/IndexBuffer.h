#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>

class IndexBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, const std::vector<uint32_t>& indices);

    void cleanup();

    vk::Buffer getBuffer() const { return m_buffer.get(); }
    uint32_t getIndexCount() const { return m_indexCount; }

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::UniqueDeviceMemory m_memory;
    uint32_t m_indexCount = 0;
    vk::UniqueBuffer m_buffer;
};
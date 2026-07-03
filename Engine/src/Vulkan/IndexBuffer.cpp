#include "Vulkan/IndexBuffer.h"
#include <cstring>
#include <stdexcept>

void IndexBuffer::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, const std::vector<uint32_t>& indices)
{
    m_indexCount = static_cast<uint32_t>(indices.size());
    vk::DeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(bufferSize);
    bufferInfo.setUsage(vk::BufferUsageFlagBits::eIndexBuffer);
    bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

    m_buffer = device.createBufferUnique(bufferInfo);

    auto memReq = device.getBufferMemoryRequirements(m_buffer.get());

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memReq.size);
    allocInfo.setMemoryTypeIndex(findMemoryType(
        physicalDevice,
        memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | 
        vk::MemoryPropertyFlagBits::eHostCoherent
    ));

    m_memory = device.allocateMemoryUnique(allocInfo);
    device.bindBufferMemory(m_buffer.get(), m_memory.get(), 0);

    void* data = device.mapMemory(m_memory.get(), 0, bufferSize);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    device.unmapMemory(m_memory.get());
}

void IndexBuffer::cleanup()
{
    m_buffer.reset();
    m_memory.reset();
}

uint32_t IndexBuffer::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memPorps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memPorps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memPorps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
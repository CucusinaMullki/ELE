#include "Vulkan/VertexBuffer.h"
#include <cstring>
#include <stdexcept>

void VertexBuffer::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, const std::vector<Vertex>& vertices)
{
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(bufferSize);
    bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer);
    bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

    m_buffer = device.createBufferUnique(bufferInfo);

    auto memReq = device.getBufferMemoryRequirements(m_buffer.get());

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memReq.size);
    allocInfo.setMemoryTypeIndex(findMemoryType(physicalDevice, memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    
    m_memory = device.allocateMemoryUnique(allocInfo);
    device.bindBufferMemory(m_buffer.get(), m_memory.get(), 0);

    void* data = device.mapMemory(m_memory.get(), 0, bufferSize);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    device.unmapMemory(m_memory.get());
}

void VertexBuffer::cleanup()
{
    m_buffer.reset();
    m_memory.reset();
}


uint32_t VertexBuffer::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
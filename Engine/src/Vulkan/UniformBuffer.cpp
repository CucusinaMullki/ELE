#include "Vulkan/UniformBuffer.h"
#include <cstring>
#include <stdexcept>

void UniformBuffer::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t frameCount)
{
    vk::DeviceSize bufferSize = sizeof(MVP);

    m_buffers.resize(frameCount);
    m_memories.resize(frameCount);
    m_mapped.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.setSize(bufferSize);
        bufferInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
        bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

        m_buffers[i] = device.createBufferUnique(bufferInfo);

        auto memReq = device.getBufferMemoryRequirements(m_buffers[i].get());

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.setAllocationSize(memReq.size);
        allocInfo.setMemoryTypeIndex(findMemoryType(
            physicalDevice,
            memReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
        ));


        m_memories[i] = device.allocateMemoryUnique(allocInfo);
        device.bindBufferMemory(m_buffers[i].get(), m_memories[i].get(), 0);

        m_mapped[i] = device.mapMemory(m_memories[i].get(), 0, bufferSize);
    }
}

void UniformBuffer::update(uint32_t frameIndex, const MVP& mvp)
{
    memcpy(m_mapped[frameIndex], &mvp, sizeof(MVP));
}

void UniformBuffer::cleanup()
{
    m_mapped.clear();
    m_memories.clear();
    m_buffers.clear();    
}

vk::DescriptorBufferInfo UniformBuffer::getDescriptorInfo(uint32_t frameIndex) const
{
    vk::DescriptorBufferInfo info{};
    info.setBuffer(m_buffers[frameIndex].get());
    info.setOffset(0);
    info.setRange(sizeof(MVP));
    return info;
}

uint32_t UniformBuffer::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
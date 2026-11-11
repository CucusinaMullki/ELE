#include "Vulkan/DecalCameraBuffer.h"
#include <cstring>

void DecalCameraBuffer::initialize(ASH::Device* device, uint32_t frameCount)
{
    m_buffers.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        ASH::BufferDesc desc{};
        desc.size = sizeof(DecalCameraData);
        desc.usage = ASH::BufferUsage::UniformBuffer;
        desc.memory = ASH::MemoryUsage::CpuToGpu;
        m_buffers[i] = device->createBuffer(desc);
    }
}

void DecalCameraBuffer::update(uint32_t frameIndex, const DecalCameraData& data)
{
    void* mapped = m_buffers[frameIndex]->map();
    memcpy(mapped, &data, sizeof(DecalCameraData));
    m_buffers[frameIndex]->unmap();
}

void DecalCameraBuffer::cleanup()
{
    m_buffers.clear();
}

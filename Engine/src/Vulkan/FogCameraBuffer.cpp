#include "Vulkan/FogCameraBuffer.h"
#include <cstring>

void FogCameraBuffer::initialize(ASH::Device* device, uint32_t frameCount)
{
    m_buffers.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        ASH::BufferDesc desc{};
        desc.size = sizeof(FogCameraData);
        desc.usage = ASH::BufferUsage::UniformBuffer;
        desc.memory = ASH::MemoryUsage::CpuToGpu;
        m_buffers[i] = device->createBuffer(desc);
    }
}

void FogCameraBuffer::update(uint32_t frameIndex, const FogCameraData& data)
{
    void* mapped = m_buffers[frameIndex]->map();
    memcpy(mapped, &data, sizeof(FogCameraData));
    m_buffers[frameIndex]->unmap();
}

void FogCameraBuffer::cleanup()
{
    m_buffers.clear();
}

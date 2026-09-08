#include "Vulkan/LightBuffer.h"
#include <cstring>

void LightBuffer::initialize(ASH::Device* device, uint32_t frameCount)
{
    m_buffers.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        ASH::BufferDesc desc{};
        desc.size = sizeof(LightData);
        desc.usage = ASH::BufferUsage::UniformBuffer;
        desc.memory = ASH::MemoryUsage::CpuToGpu;
        m_buffers[i] = device->createBuffer(desc);
    }
}

void LightBuffer::update(uint32_t frameIndex, const LightData& data)
{
    void* mapped = m_buffers[frameIndex]->map();
    memcpy(mapped, &data, sizeof(LightData));
    m_buffers[frameIndex]->unmap();
}

void LightBuffer::cleanup()
{
    m_buffers.clear();
}
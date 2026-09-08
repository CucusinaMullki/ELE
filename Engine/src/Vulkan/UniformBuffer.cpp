#include "Vulkan/UniformBuffer.h"
#include <cstring>

void UniformBuffer::initialize(ASH::Device* device, uint32_t frameCount)
{
    m_buffers.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        ASH::BufferDesc desc{};
        desc.size = sizeof(MVP);
        desc.usage = ASH::BufferUsage::UniformBuffer;
        desc.memory = ASH::MemoryUsage::CpuToGpu;
        m_buffers[i] = device->createBuffer(desc);
    }
}

void UniformBuffer::update(uint32_t frameIndex, const MVP& mvp)
{
    void* mapped = m_buffers[frameIndex]->map();
    memcpy(mapped, &mvp, sizeof(MVP));
    m_buffers[frameIndex]->unmap();
}

void UniformBuffer::cleanup()
{
    m_buffers.clear();
}
#include "Vulkan/BoneMatrixBuffer.h"
#include <cstring>

void BoneMatrixBuffer::initialize(ASH::Device* device, uint32_t frameCount)
{
    m_buffers.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; i++)
    {
        ASH::BufferDesc desc{};
        desc.size = sizeof(BoneMatrixData);
        desc.usage = ASH::BufferUsage::UniformBuffer;
        desc.memory = ASH::MemoryUsage::CpuToGpu;
        m_buffers[i] = device->createBuffer(desc);
    }
}

void BoneMatrixBuffer::update(uint32_t frameIndex, const BoneMatrixData& data)
{
    void* mapped = m_buffers[frameIndex]->map();
    memcpy(mapped, &data, sizeof(BoneMatrixData));
    m_buffers[frameIndex]->unmap();
}

void BoneMatrixBuffer::cleanup()
{
    m_buffers.clear();
}
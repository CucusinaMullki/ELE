#include "Vulkan/IndexBuffer.h"
#include <cstring>

void IndexBuffer::initialize(ASH::Device* device, const std::vector<uint32_t>& indices)
{
    m_indexCount = static_cast<uint32_t>(indices.size());

    ASH::BufferDesc desc{};
    desc.size = sizeof(uint32_t) * indices.size();
    desc.usage = ASH::BufferUsage::IndexBuffer;
    desc.memory = ASH::MemoryUsage::CpuToGpu;

    m_buffer = device->createBuffer(desc);

    void* data = m_buffer->map();
    memcpy(data, indices.data(), desc.size);
    m_buffer->unmap();
}

void IndexBuffer::cleanup()
{
    m_buffer.reset();
}
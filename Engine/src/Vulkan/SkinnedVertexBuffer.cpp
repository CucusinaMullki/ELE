#include "Vulkan/SkinnedVertexBuffer.h"
#include <cstring>

void SkinnedVertexBuffer::initialize(ASH::Device* device, const std::vector<SkinnedVertex>& vertices)
{
    m_vertexCount = static_cast<uint32_t>(vertices.size());

    ASH::BufferDesc desc{};
    desc.size = sizeof(SkinnedVertex) * vertices.size();
    desc.usage = ASH::BufferUsage::VertexBuffer;
    desc.memory = ASH::MemoryUsage::CpuToGpu;

    m_buffer = device->createBuffer(desc);

    void* data = m_buffer->map();
    memcpy(data, vertices.data(), desc.size);
    m_buffer->unmap();
}

void SkinnedVertexBuffer::cleanup()
{
    m_buffer.reset();
}

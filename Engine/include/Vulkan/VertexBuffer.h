#pragma once
#include "ASH/ASH.h"
#include "Vulkan/Vertex.h"
#include <vector>
#include <memory>

class VertexBuffer
{
public:
    void initialize(ASH::Device* device, const std::vector<Vertex>& vertices);

    void cleanup();

    ASH::Buffer* getBuffer() const { return m_buffer.get(); }
    uint32_t getVertexCount() const { return m_vertexCount; }

private:
    std::unique_ptr<ASH::Buffer> m_buffer;
    uint32_t m_vertexCount = 0;
};
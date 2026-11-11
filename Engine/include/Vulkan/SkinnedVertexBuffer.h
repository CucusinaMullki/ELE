#pragma once
#include "ASH/ASH.h"
#include "Vulkan/SkinnedVertex.h"
#include <memory>
#include <vector>

class SkinnedVertexBuffer
{
public:
    void initialize(ASH::Device* device, const std::vector<SkinnedVertex>& vertices);
    void cleanup();
    ASH::Buffer* getBuffer() const { return m_buffer.get(); }
    uint32_t getVertexCount() const { return m_vertexCount; }

private:
    std::unique_ptr<ASH::Buffer> m_buffer;
    uint32_t m_vertexCount = 0;
};

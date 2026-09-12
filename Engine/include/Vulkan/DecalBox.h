#pragma once
#include "ASH/ASH.h"
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/IndexBuffer.h"

class DecalBox
{
public:
    void initialize(ASH::Device* device);
    void cleanup();

    void bind(ASH::CommandBuffer* cmd) const;
    void draw(ASH::CommandBuffer* cmd) const;

private:
    VertexBuffer m_vertexBuffer;
    IndexBuffer m_indexBuffer;
    uint32_t m_indexCount = 0;
};
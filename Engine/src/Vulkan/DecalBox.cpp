#include "Vulkan/DecalBox.h"
#include "Vulkan/Vertex.h"
#include <vector>

void DecalBox::initialize(ASH::Device* device)
{
    std::vector<Vertex> vertices = {
        {{-0.5f,-0.5f,-0.5f}, {1,1,1}, {0,0}, {0,0,-1}, {1,0,0}, {0,0}},
        {{ 0.5f,-0.5f,-0.5f}, {1,1,1}, {1,0}, {0,0,-1}, {1,0,0}, {0,0}},
        {{ 0.5f, 0.5f,-0.5f}, {1,1,1}, {1,1}, {0,0,-1}, {1,0,0}, {0,0}},
        {{-0.5f, 0.5f,-0.5f}, {1,1,1}, {0,1}, {0,0,-1}, {1,0,0}, {0,0}},
        {{-0.5f,-0.5f, 0.5f}, {1,1,1}, {0,0}, {0,0, 1}, {1,0,0}, {0,0}},
        {{ 0.5f,-0.5f, 0.5f}, {1,1,1}, {1,0}, {0,0, 1}, {1,0,0}, {0,0}},
        {{ 0.5f, 0.5f, 0.5f}, {1,1,1}, {1,1}, {0,0, 1}, {1,0,0}, {0,0}},
        {{-0.5f, 0.5f, 0.5f}, {1,1,1}, {0,1}, {0,0, 1}, {1,0,0}, {0,0}},
    };

    std::vector<uint32_t> indices = {
        0,1,2, 2,3,0,
        4,6,5, 6,4,7,
        4,0,3, 3,7,4,
        1,5,6, 6,2,1,
        3,2,6, 6,7,3,
        4,5,1, 1,0,4
    };

    m_vertexBuffer.initialize(device, vertices);
    m_indexBuffer.initialize(device, indices);
    m_indexCount = static_cast<uint32_t>(indices.size());
}

void DecalBox::cleanup()
{
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
}

void DecalBox::bind(ASH::CommandBuffer* cmd) const
{
    cmd->bindVertexBuffer(m_vertexBuffer.getBuffer(), 0, 0);
    cmd->bindIndexBuffer(m_indexBuffer.getBuffer(), 0, ASH::IndexType::UInt32);
}

void DecalBox::draw(ASH::CommandBuffer* cmd) const
{
    cmd->drawIndexed(m_indexCount, 1, 0, 0, 0);
}
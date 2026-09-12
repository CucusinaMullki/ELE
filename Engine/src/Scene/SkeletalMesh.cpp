#include "Scene/SkeletalMesh.h"

void SkeletalMesh::loadFromData(ASH::Device* device,
    const std::vector<SkinnedVertex>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<Submesh>& submeshes)
{
    m_vertexBuffer.initialize(device, vertices);
    m_indexBuffer.initialize(device, indices);
    m_submeshes = submeshes;
}

void SkeletalMesh::cleanup()
{
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
}

void SkeletalMesh::bind(ASH::CommandBuffer* cmd) const
{
    cmd->bindVertexBuffer(m_vertexBuffer.getBuffer(), 0, 0);
    cmd->bindIndexBuffer(m_indexBuffer.getBuffer(), 0, ASH::IndexType::UInt32);
}

void SkeletalMesh::drawSubmesh(ASH::CommandBuffer* cmd, const Submesh& sm) const
{
    cmd->drawIndexed(sm.indexCount, 1, sm.firstIndex, 0, 0);
}
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
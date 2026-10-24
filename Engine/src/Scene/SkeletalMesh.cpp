#include "Scene/SkeletalMesh.h"
#include <cfloat>

void SkeletalMesh::loadFromData(ASH::Device* device,
    const std::vector<SkinnedVertex>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<Submesh>& submeshes)
{
    m_vertexBuffer.initialize(device, vertices);
    m_indexBuffer.initialize(device, indices);
    m_submeshes = submeshes;
    m_cpuVertices = vertices;
    m_cpuIndices = indices;

    if (!vertices.empty())
    {
        glm::vec3 minB(FLT_MAX), maxB(-FLT_MAX);
        for (const auto& v : vertices)
        {
            minB = glm::min(minB, v.pos);
            maxB = glm::max(maxB, v.pos);
        }
        m_boundsMin = minB;
        m_boundsMax = maxB;
    }
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

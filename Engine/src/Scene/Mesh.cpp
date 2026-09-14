#include "Scene/Mesh.h"
#include "Core/Model.h"
#include "Core/GltfModel.h"

void Mesh::load(ASH::Device* device, const std::string& path, bool keepCpuData)
{
    GltfModel model;
    model.load(path);
    m_vertexBuffer.initialize(device, model.getVertices());
    m_indexBuffer.initialize(device, model.getIndices());
    m_submeshes = model.getSubmesh();
    if (keepCpuData)
    {
        m_cpuVertices = model.getVertices();
        m_cpuIndices = model.getIndices();
    }
}

void Mesh::cleanup()
{
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
    m_submeshes.clear();
}

void Mesh::bind(ASH::CommandBuffer* cmd) const
{
    cmd->bindVertexBuffer(m_vertexBuffer.getBuffer(), 0, 0);
    cmd->bindIndexBuffer(m_indexBuffer.getBuffer(), 0, ASH::IndexType::UInt32);
}

void Mesh::drawSubmesh(ASH::CommandBuffer* cmd, const Submesh& sm) const
{
    cmd->drawIndexed(sm.indexCount, 1, sm.firstIndex, 0, 0);
}

void Mesh::computeBounds(const std::vector<Vertex>& vertices)
{
    if (vertices.empty()) { m_boundsCenter = glm::vec3(0.0f); m_boundRadius = 0.0f; return; }
    glm::vec3 minB(FLT_MAX), maxB(-FLT_MAX);
    for (const auto& v : vertices)
    {
        minB = glm::min(minB, v.pos);
        maxB = glm::max(maxB, v.pos);
    }
    m_boundsCenter = (minB + maxB) * 0.5f;
    m_boundRadius = glm::length(maxB - m_boundsCenter);
    m_boundsMin = minB;
    m_boundsMax = maxB;
}

void Mesh::loadFromData(ASH::Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<Submesh>& submeshes,
        bool keepCpuData)
{
    m_vertexBuffer.initialize(device, vertices);
    m_indexBuffer.initialize(device, indices);
    m_submeshes = submeshes;
    if (keepCpuData)
    {
        m_cpuVertices = vertices;
        m_cpuIndices = indices;
    }
}
#include "Scene/Mesh.h"
#include "Core/Model.h"
#include "Core/GltfModel.h"
#include <vulkan/VulkanBuffer.h>

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

void Mesh::bind(vk::CommandBuffer cmd) const
{
    auto* vulkanVertexBuf = static_cast<ASH::vulkan::VulkanBuffer*>(m_vertexBuffer.getBuffer());
    auto* vulkanIndexBuf = static_cast<ASH::vulkan::VulkanBuffer*>(m_indexBuffer.getBuffer());

    vk::Buffer vertexBuffers[] = { vulkanVertexBuf->getHandle() };
    vk::DeviceSize offsets[] = { 0 };
    cmd.bindVertexBuffers(0, vertexBuffers, offsets);
    cmd.bindIndexBuffer(vulkanIndexBuf->getHandle(), 0, vk::IndexType::eUint32);
}

void Mesh::drawSubmesh(vk::CommandBuffer cmd, const Submesh& sm) const
{
    cmd.drawIndexed(sm.indexCount, 1, sm.firstIndex, 0, 0);
}

void Mesh::computeBounds(const std::vector<Vertex>& vertices)
{
    if (vertices.empty()) { m_boundsCenter = glm::vec3(0.0f); m_boundRadius = 0.0f; }
    glm::vec3 minB(FLT_MAX), maxB(-FLT_MAX);
    for (const auto& v : vertices)
    {
        minB = glm::min(minB, v.pos);
        maxB = glm::max(maxB, v.pos);
    }
    m_boundsCenter = (minB + maxB) * 0.5f;
    m_boundRadius = glm::length(maxB - m_boundsCenter);
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
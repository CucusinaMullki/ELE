#include "Scene/SkeletalMesh.h"

void SkeletalMesh::loadFromData(vk::Device device, vk::PhysicalDevice physicalDevice,
    const std::vector<SkinnedVertex>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<Submesh>& submeshes)
{
    m_vertexBuffer.initialize(device, physicalDevice, vertices);
    m_indexBuffer.initialize(device, physicalDevice, indices);
    m_submeshes = submeshes;
}

void SkeletalMesh::cleanup()
{
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
}

void SkeletalMesh::bind(vk::CommandBuffer cmd) const
{
    vk::Buffer vertexBuffers[] = { m_vertexBuffer.getBuffer() };
    vk::DeviceSize offsets[] = { 0 };
    cmd.bindVertexBuffers(0, vertexBuffers, offsets);
    cmd.bindIndexBuffer(m_indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
}

void SkeletalMesh::drawSubmesh(vk::CommandBuffer cmd, const Submesh& sm) const
{
    cmd.drawIndexed(sm.indexCount, 1, sm.firstIndex, 0, 0);
}
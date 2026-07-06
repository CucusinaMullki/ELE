#include "Scene/Mesh.h"

void Mesh::load(vk::Device device, vk::PhysicalDevice physicalDevice, const std::string& modelPath)
{
    Model model;
    model.load(modelPath);

    m_vertexBuffer.initialize(device, physicalDevice, model.getVertices());
    m_indexBuffer.initialize(device, physicalDevice, model.getIndices());
}

void Mesh::cleanup()
{
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
}

void Mesh::bind(vk::CommandBuffer cmd) const
{
    vk::Buffer vertexBuffers[] = { m_vertexBuffer.getBuffer() };
    vk::DeviceSize offsets[] = { 0 };
    cmd.bindVertexBuffers(0, vertexBuffers, offsets);
    cmd.bindIndexBuffer(m_indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
}

void Mesh::draw(vk::CommandBuffer cmd) const
{
    cmd.drawIndexed(m_indexBuffer.getIndexCount(), 1, 0, 0, 0);
}
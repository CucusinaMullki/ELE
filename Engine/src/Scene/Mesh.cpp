#include "Scene/Mesh.h"
#include "Core/Model.h"
#include "Core/GltfModel.h"

void Mesh::load(vk::Device device, vk::PhysicalDevice physicalDevice, const std::string& path)
{
    bool isGltf = path.size() > 5 &&
                  (path.substr(path.size() - 5) == ".gltf" || path.substr(path.size() - 4) == ".glb");

    if (isGltf)
    {
        GltfModel model;
        model.load(path);
        m_vertexBuffer.initialize(device, physicalDevice, model.getVertices());
        m_indexBuffer.initialize(device, physicalDevice, model.getIndices());
        m_submeshes = model.getSubmesh();
    }
    else
    {
        Model model;
        model.load(path);
        m_vertexBuffer.initialize(device, physicalDevice, model.getVertices());
        m_indexBuffer.initialize(device, physicalDevice, model.getIndices());
        m_submeshes = model.getSubmesh();
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
    vk::Buffer vertexBuffers[] = { m_vertexBuffer.getBuffer() };
    vk::DeviceSize offsets[] = { 0 };
    cmd.bindVertexBuffers(0, vertexBuffers, offsets);
    cmd.bindIndexBuffer(m_indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
}

void Mesh::drawSubmesh(vk::CommandBuffer cmd, const Submesh& sm) const
{
    cmd.drawIndexed(sm.indexCount, 1, sm.firstIndex, 0, 0);
}

void Mesh::loadFromData(vk::Device device, vk::PhysicalDevice physicalDevice,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<Submesh>& submeshes)
{
    m_vertexBuffer.initialize(device, physicalDevice, vertices);
    m_indexBuffer.initialize(device, physicalDevice, indices);
    m_submeshes = submeshes;
}
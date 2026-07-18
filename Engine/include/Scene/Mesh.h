#pragma once
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/IndexBuffer.h"
#include "Scene/Submesh.h"
#include <string>
#include <vector>

class Mesh
{
public:
    void load(vk::Device device, vk::PhysicalDevice physicalDevice, const std::string& path);
    void loadFromData(vk::Device device, vk::PhysicalDevice physicalDevice,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<Submesh>& submeshes);
    
    void cleanup();

    void bind(vk::CommandBuffer cmd) const;
    void drawSubmesh(vk::CommandBuffer cmd, const Submesh& sm) const;

    const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }

private:
    VertexBuffer m_vertexBuffer;
    IndexBuffer  m_indexBuffer;
    std::vector<Submesh> m_submeshes;
};
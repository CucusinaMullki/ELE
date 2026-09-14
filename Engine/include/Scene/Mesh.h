#pragma once
#include "ASH/ASH.h"
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/IndexBuffer.h"
#include "Scene/Submesh.h"
#include <string>
#include <vector>

class Mesh
{
public:
    void load(ASH::Device* device, const std::string& path, bool keepCpuData = false);
    void loadFromData(ASH::Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<Submesh>& submeshes,
        bool keepCpuData = false);

    void cleanup();
    void bind(ASH::CommandBuffer* cmd) const;
    void drawSubmesh(ASH::CommandBuffer* cmd, const Submesh& sm) const;
    const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }
    bool hasCpuData() const { return !m_cpuVertices.empty(); }
    const std::vector<Vertex>& getCpuVertices() const { return m_cpuVertices; }
    const std::vector<uint32_t>& getCpuIndices() const { return m_cpuIndices; }
    glm::vec3 getLocalBoundCentre() const { return m_boundsCenter; }
    float getLocalBoundsRadius() const { return m_boundRadius; }
    glm::vec3 getLocalBoundsMin() const { return m_boundsMin; }
    glm::vec3 getLocalBoundsMax() const { return m_boundsMax; }

private:
    glm::vec3 m_boundsCenter{0.0f};
    float m_boundRadius = 0.0f;
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};
    void computeBounds(const std::vector<Vertex>& vertices);
    VertexBuffer m_vertexBuffer;
    IndexBuffer  m_indexBuffer;
    std::vector<Submesh> m_submeshes;
    std::vector<Vertex> m_cpuVertices;
    std::vector<uint32_t> m_cpuIndices;
};
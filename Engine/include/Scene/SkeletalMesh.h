#pragma once
#include "ASH/ASH.h"
#include "Vulkan/SkinnedVertexBuffer.h"
#include "Vulkan/IndexBuffer.h"
#include "Scene/Submesh.h"
#include "Scene/Skeleton.h"
#include <memory>
#include <vector>

class SkeletalMesh
{
public:
    void loadFromData(ASH::Device* device,
        const std::vector<SkinnedVertex>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<Submesh>& submeshes);

    void cleanup();

    ~SkeletalMesh() { cleanup(); }

    void bind(ASH::CommandBuffer* cmd) const;
    void drawSubmesh(ASH::CommandBuffer* cmd, const Submesh& sm) const;

    const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }

    glm::vec3 getLocalBoundsMin() const { return m_boundsMin; }
    glm::vec3 getLocalBoundsMax() const { return m_boundsMax; }

    void setSkeleton(std::shared_ptr<Skeleton> skeleton) { m_skeleton = skeleton; }
    std::shared_ptr<Skeleton> getSkeleton() const { return m_skeleton; }

    const std::vector<SkinnedVertex>& getCpuVertices() const { return m_cpuVertices; }
    const std::vector<uint32_t>& getCpuIndices() const { return m_cpuIndices; }
private:
    SkinnedVertexBuffer m_vertexBuffer;
    IndexBuffer m_indexBuffer;
    std::vector<Submesh> m_submeshes;
    std::shared_ptr<Skeleton> m_skeleton;

    std::vector<SkinnedVertex> m_cpuVertices;
    std::vector<uint32_t> m_cpuIndices;

    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};
};

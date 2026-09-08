#pragma once
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

    void bind(vk::CommandBuffer cmd) const;
    void drawSubmesh(vk::CommandBuffer cmd, const Submesh& sm) const;

    const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }

    void setSkeleton(std::shared_ptr<Skeleton> skeleton) { m_skeleton = skeleton; }
    std::shared_ptr<Skeleton> getSkeleton() const { return m_skeleton; }

private:
    SkinnedVertexBuffer m_vertexBuffer;
    IndexBuffer m_indexBuffer;
    std::vector<Submesh> m_submeshes;
    std::shared_ptr<Skeleton> m_skeleton;
};
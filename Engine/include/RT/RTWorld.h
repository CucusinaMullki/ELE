#pragma once
#include "RT/RTContext.h"
#include "RT/Blas.h"
#include "Vulkan/Vertex.h"
#include "Scene/Mesh.h"
#include "RT/RTShadowPass.h"
#include "Scene/Scene.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace RT
{

class World
{
public:
    ~World() { shutdown(); }

    bool init(ASH::Device* device);
    void shutdown();

    ShadowPass& shadows() { return m_shadows; }
    void recordShadows(ASH::CommandBuffer* cmd, uint32_t frame) { if (m_ready) m_shadows.record(m_ctx, cmd, frame); }

    bool addMesh(const void* key, const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices, const std::vector<Submesh>& submeshes);

    bool sync(const ::Scene& scene);

    Context& context() { return m_ctx; }
    VkAccelerationStructureKHR tlas() const { return m_tlas; }
    uint32_t instanceCount() const { return m_instanceCount; }

private:
    void destroyTlas();

    ShadowPass m_shadows;
    Context m_ctx;
    bool m_ready = false;
    std::unordered_map<const void*, std::unique_ptr<Blas>> m_blas;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
    std::unique_ptr<ASH::Buffer> m_tlasStorage;
    uint32_t m_instanceCount = 0;
    uint32_t m_syncedVersion = ~0u;
};

}
#pragma once
#include "RT/RTContext.h"
#include "RT/Blas.h"
#include "Vulkan/Vertex.h"
#include "Scene/Mesh.h"
#include "RT/RTShadowPass.h"
#include "Components/SkeletalMeshComponent.h"
#include "RT/RTReflectionPass.h"
#include "Scene/Scene.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include "RT/SkinnedBlas.h"
#include "RT/RTSkinPass.h"

namespace RT
{

struct GeoEntry
{
    uint64_t vertexAddr;
    uint64_t indexAddr;
    uint32_t firstIndex;
    uint32_t materialIndex;
    uint32_t objectIndex;
    uint32_t pad;
};

class World
{
public:
    ~World() { shutdown(); }

    bool init(ASH::Device* device);
    void shutdown();

    ShadowPass& shadows() { return m_shadows; }
    void recordShadows(ASH::CommandBuffer* cmd, uint32_t frame) { if (m_ready) m_shadows.record(m_ctx, cmd, frame); }

    ReflectionPass& reflections() { return m_reflections; }
    void recordReflections(ASH::CommandBuffer* cmd, uint32_t frame) { if (m_ready) m_reflections.record(m_ctx, cmd, frame); }

    void setProbeBuffers(ASH::Buffer* data, ASH::Buffer* info, uint32_t count)
    { if (m_ready) m_reflections.setProbeBuffers(m_ctx, data, info, count); }
    void recordProbes(ASH::CommandBuffer* cmd, uint32_t frame)
    { if (m_ready) m_reflections.recordProbes(m_ctx, cmd, frame); }

    bool addMesh(const void* key, const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices, const std::vector<Submesh>& submeshes);

    bool sync(const ::Scene& scene);
    void updateDynamic(const ::Scene& scene, ASH::CommandBuffer* cmd, uint32_t frame);
    void updateSkinned(const ::Scene& scene, ASH::CommandBuffer* cmd, uint32_t frame);

    void setDefaultTexture(ASH::Texture* albedo, ASH::Texture* normal);
    uint32_t registerMaterial(const void* key, ASH::Texture* albedo, ASH::Texture* normal);

    Context& context() { return m_ctx; }
    VkAccelerationStructureKHR tlas() const { return m_tlas; }
    uint32_t instanceCount() const { return m_instanceCount; }
    ASH::Buffer* geometryTable() const { return m_geoTable.get(); }
    uint32_t geometryCount() const { return m_geoCount; }

private:
    void destroyTlas();

    ShadowPass m_shadows;
    ReflectionPass m_reflections;
    Context m_ctx;
    bool m_ready = false;
    std::unordered_map<const void*, std::unique_ptr<Blas>> m_blas;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
    std::unique_ptr<ASH::Buffer> m_tlasStorage;
    uint32_t m_instanceCount = 0;
    uint32_t m_syncedVersion = ~0u;
    std::unique_ptr<ASH::Buffer> m_geoTable;
    uint32_t m_geoCount = 0;
    std::unordered_map<const void*, uint32_t> m_matSlots;
    std::vector<VkImageView> m_albedoViews;
    std::vector<VkImageView> m_normalViews;
    bool m_dirty = false;
    VkImageView albedoView(ASH::Texture* t);
    static VkImageView normalView(ASH::Texture* t);

    static constexpr uint32_t kFrames = 3;
    struct DynInst { uint32_t inst; uint32_t obj; };

    decltype(m_geoTable) m_instBuf[kFrames];
    decltype(m_geoTable) m_tlasScratch;
    VkDeviceAddress m_scratchAddr = 0;
    std::vector<DynInst> m_dyn;

    struct SkinInst { SkinnedBlas blas; VkDescriptorSet sets[kFrames] = {}; };
    struct SkinRef { GameObject* obj; SkinInst* si; };
    SkinInst* getOrCreateSkinned(GameObject& o, SkeletalMeshComponent& sk);
    SkinPass m_skin;
    std::unordered_map<const void*, std::unique_ptr<SkinInst>> m_skinned;
    std::vector<SkinRef> m_skinList;
};

}

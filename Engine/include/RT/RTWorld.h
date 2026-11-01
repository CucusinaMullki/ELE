#pragma once
#include "RT/RTContext.h"
#include "RT/RTMaster.h"
#include "RT/SkinnedBlas.h"
#include "RT/Blas.h"
#include "RT/RTSkinPass.h"
#include "Components/SkeletalMeshComponent.h"
#include "Scene/Scene.h"
#include <ASH/ASH.h>
#include <memory>
#include <unordered_map>
#include <vector>

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

static constexpr uint32_t kFrames = 2;

class World
{
public:
    bool init(ASH::Device* device, uint32_t frames, uint32_t w, uint32_t h);
    void shutdown();

    ASH::Texture* shadowImage(uint32_t frame) const;
    ASH::Texture* reflectionImage(uint32_t frame) const;
    ASH::Texture* aoImage(uint32_t frame) const;
    ASH::Texture* giImage(uint32_t frame) const;

    void recordShadows(ASH::CommandBuffer* cmd, uint32_t frame);
    void recordReflections(ASH::CommandBuffer* cmd, uint32_t frame);
    void recordAO(ASH::CommandBuffer* cmd, uint32_t frame);
    void recordGI(ASH::CommandBuffer* cmd, uint32_t frame);
    void recordAll(ASH::CommandBuffer* cmd, uint32_t frame);
    void recordDenoise(ASH::CommandBuffer* cmd, uint32_t frame, const float* view, const float* proj);
    void resize(uint32_t w, uint32_t h);

    bool addMesh(const void* key, const std::vector<Vertex>& vertices,
                 const std::vector<uint32_t>& indices, const std::vector<Submesh>& submeshes);
    void updateDynamic(const ::Scene& scene, ASH::CommandBuffer* cmd, uint32_t frame);
    void updateSkinned(const ::Scene& scene, ASH::CommandBuffer* cmd, uint32_t frame);
    bool sync(const ::Scene& scene);
    void setDefaultTexture(ASH::Texture* albedo, ASH::Texture* normal);

    void bindFrameBuffers(uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights);

    uint32_t registerMaterial(const void* key, ASH::Texture* albedo, ASH::Texture* normal);

    ShadowPass& shadows() { return m_master->shadows(); }
    ReflectionPass& reflections() { return m_master->reflections(); }
    AOPass& ao() { return m_master->ao(); }
    GIPass& gi() { return m_master->gi(); }

    const ShadowPass& shadows() const { return m_master->shadows(); }
    const ReflectionPass& reflections() const { return m_master->reflections(); }
    const AOPass& ao() const { return m_master->ao(); }
    const GIPass& gi() const { return m_master->gi(); }

    uint32_t instanceCount() const { return m_instanceCount; }
    uint32_t geometryCount() const { return m_geoCount; }

    Context& context() { return m_ctx; }
    const Context& context() const { return m_ctx; }

private:
    void destroyTlas();

    Context m_ctx;
    std::unique_ptr<Master> m_master;
    SkinPass m_skin;

    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
    std::unique_ptr<ASH::Buffer> m_tlasStorage;
    std::unique_ptr<ASH::Buffer> m_tlasScratch;
    uint64_t m_scratchAddr = 0;

    std::unique_ptr<ASH::Buffer> m_geoTable;
    uint32_t m_geoCount = 0;

    std::unique_ptr<ASH::Buffer> m_instBuf[kFrames];
    uint32_t m_instanceCount = 0;

    struct DynInst { uint32_t inst; uint32_t obj; };
    std::vector<DynInst> m_dyn;

    struct SkinInst {
        SkinnedBlas blas;
        VkDescriptorSet sets[kFrames]{};
    };
    struct SkinRef { GameObject* obj; SkinInst* si; };
    std::vector<SkinRef> m_skinList;
    std::unordered_map<const void*, std::unique_ptr<SkinInst>> m_skinned;
    SkinInst* getOrCreateSkinned(GameObject& o, SkeletalMeshComponent& sk);

    std::unordered_map<const void*, std::unique_ptr<Blas>> m_blas;

    std::vector<VkImageView> m_albedoViews;
    std::vector<VkImageView> m_normalViews;
    std::unordered_map<const void*, uint32_t> m_matSlots;

    uint64_t m_syncedVersion = 0;
    bool m_dirty = false;
    bool m_ready = false;

    static VkImageView albedoView(ASH::Texture* t);
    static VkImageView normalView(ASH::Texture* t);
};

}
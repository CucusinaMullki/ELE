#pragma once
#include "RT/RTContext.h"
#include "RT/RTDenoiser.h"
#include "RT/RTProbeUpdate.h"
#include <ASH/ASH.h>
#include <memory>
#include <vector>

namespace RT
{

class ReflectionPass
{
public:
    bool createPipeline(Context& ctx);
    bool createResources(Context& ctx, uint32_t frames, uint32_t w, uint32_t h);
    void resize(Context& ctx, uint32_t w, uint32_t h);
    void bindFrameBuffers(Context& ctx, uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights);
    void setTlas(Context& ctx, VkAccelerationStructureKHR tlas);
    void setGeometry(Context& ctx, ASH::Buffer* geoTable);
    void record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame);
    void destroy(Context& ctx);
    void recordDenoise(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame, const float* view, const float* proj);
    ASH::Texture* denoisedDiffuse() const { return m_denoiser.outDiff(); }
    ASH::Texture* image(uint32_t frame) const { return frame < m_img.size() ? m_img[frame].get() : nullptr; }
    ASH::Texture* gi(uint32_t frame) const
    {
        if (m_denoiser.ready()) return m_denoiser.outDiff();
        return frame < m_gi.size() ? m_gi[frame].get() : nullptr;
    }
    ASH::Texture* ao(uint32_t frame) const { return frame < m_ao.size() ? m_ao[frame].get() : nullptr; }
    ASH::Texture* nrdDiff(uint32_t frame) const { return frame < m_nrdDiff.size() ? m_nrdDiff[frame].get() : nullptr; }
    ASH::Texture* nrdNr(uint32_t frame) const { return frame < m_nrdNr.size() ? m_nrdNr[frame].get() : nullptr; }
    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }
    static constexpr uint32_t kMaxTextures = 1024;
    void setTextures(Context& ctx, const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal);
    static constexpr uint32_t kRaysPerProbe = 64;
    static constexpr uint32_t kProbesPerFrame = 2048;
    static constexpr bool kProbeUpdate = false;
    bool createProbePipeline(Context& ctx);
    void setProbeBuffers(Context& ctx, ASH::Buffer* probeData, ASH::Buffer* probeInfo, uint32_t probeCount);
    void recordProbes(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame);
    ASH::Buffer* probeRays() const { return m_rayBuf.get(); }
    uint32_t lastProbeOffset() const { return m_lastProbeOffset; }
    uint32_t lastProbeBatch() const { return m_lastProbeBatch; }

private:
    bool createSbt(Context& ctx);
    void createImages(Context& ctx);
    void updateSet(Context& ctx, uint32_t frame);
    void updateTextures(Context& ctx, uint32_t frame);
    bool createProbeSbt(Context& ctx);

    std::vector<std::unique_ptr<ASH::Texture>> m_nrdDiff, m_nrdNr, m_nrdVz;
    Denoiser m_denoiser;

    VkPipeline m_probePipeline = VK_NULL_HANDLE;
    std::unique_ptr<ASH::Buffer> m_probeSbt;
    VkStridedDeviceAddressRegionKHR m_pRgen{}, m_pMiss{}, m_pHit{};
    std::unique_ptr<ASH::Buffer> m_rayBuf;
    VkBuffer m_probeDataBuf = VK_NULL_HANDLE, m_probeInfoBuf = VK_NULL_HANDLE;
    uint32_t m_probeCount = 0, m_probeOffset = 0, m_lastProbeOffset = 0, m_lastProbeBatch = 0;
    ProbeUpdatePass m_probeUpdate;

    VkSampler m_sampler = VK_NULL_HANDLE;
    std::vector<VkImageView> m_views;
    std::vector<VkImageView> m_nviews;

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_sets;

    std::vector<std::unique_ptr<ASH::Texture>> m_img;
    std::vector<std::unique_ptr<ASH::Texture>> m_ao;
    std::vector<std::unique_ptr<ASH::Texture>> m_gi;
    std::vector<ASH::Buffer*> m_cam, m_light;
    VkBuffer m_geoBuf = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
    uint32_t m_frameCounter = 0;
    std::vector<uint8_t> m_giState;

    std::unique_ptr<ASH::Buffer> m_sbt;
    VkStridedDeviceAddressRegionKHR m_rgen{}, m_miss{}, m_hit{}, m_callable{};
    uint32_t m_frames = 0, m_w = 0, m_h = 0;
};

}
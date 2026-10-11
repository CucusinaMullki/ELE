#pragma once
#include "RT/RTContext.h"
#include <ASH/ASH.h>
#include <memory>
#include <vector>

namespace RT {

class ShadowPass {
public:
    bool createPipeline(Context& ctx);
    bool createResources(Context& ctx, uint32_t frames, uint32_t w, uint32_t h);
    void bindFrameBuffers(Context& ctx, uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights);
    void setTlas(Context& ctx, VkAccelerationStructureKHR tlas);
    void record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame);
    void destroy(Context& ctx);
    void resize(Context& ctx, uint32_t w, uint32_t h);

    ASH::Texture* mask(uint32_t frame) const { return frame < m_mask.size() ? m_mask[frame].get() : nullptr; }

private:
    bool createSbt(Context& ctx);
    void updateSet(Context& ctx, uint32_t frame);
    void createMasks(Context& ctx);

    VkDescriptorSetLayout m_setLayout  = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;

    std::unique_ptr<ASH::Buffer> m_sbt;
    VkStridedDeviceAddressRegionKHR m_rgen{}, m_miss{}, m_hit{}, m_callable{};

    uint32_t m_frames = 0, m_w = 0, m_h = 0;
    std::vector<std::unique_ptr<ASH::Texture>> m_mask;
    std::vector<VkDescriptorSet> m_sets;
    std::vector<ASH::Buffer*> m_cam, m_light;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
};

}
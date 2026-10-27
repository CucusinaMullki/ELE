#pragma once
#include "RT/RTContext.h"
#include <ASH/Texture.h>
#include <ASH/Buffer.h>
#include <memory>
#include <vector>

namespace ASH { class CommandBuffer; }
namespace nrd { struct Instance; }

namespace RT
{
class Denoiser
{
public:
    bool create(Context& ctx, uint32_t w, uint32_t h, uint32_t frames);
    void destroy(Context& ctx);
    bool ready() const { return m_ready; }

    void record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame,
        const float* view, const float* proj,
        ASH::Texture* inDiff, ASH::Texture* inNormalRough, ASH::Texture* inViewZ);
    ASH::Texture* outDiff() const { return m_out.get(); }

private:
    bool m_ready = false;
    uint32_t m_w = 0, m_h = 0, m_frames = 0;

    nrd::Instance* m_inst = nullptr;

    std::vector<std::unique_ptr<ASH::Texture>> m_perm;
    std::vector<std::unique_ptr<ASH::Texture>> m_trans;

    VkSampler m_samplers[2] = {};
    uint32_t m_samplersNum = 0;

    VkDescriptorSetLayout m_cbLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_cbPool = VK_NULL_HANDLE;
    VkDescriptorSet m_cbSet = VK_NULL_HANDLE;

    std::vector<VkDescriptorSetLayout> m_resLayouts;
    std::vector<VkPipelineLayout> m_pipeLayouts;
    std::vector<VkPipeline> m_pipelines;
    std::vector<VkDescriptorPool> m_pools;

    std::unique_ptr<ASH::Texture> m_out;
    std::unique_ptr<ASH::Buffer> m_cb;
    uint32_t m_cbStride = 0;
    uint32_t m_slots = 0;
    uint32_t m_cbBinding = 0;
    bool m_needInit = true;
    bool m_hasPrev = false;
    uint32_t m_frameIndex = 0;
    float m_prevView[16] = {};
    float m_prevProj[16] = {};
};
}
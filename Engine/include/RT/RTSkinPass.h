#pragma once
#include "RT/RTContext.h"

namespace RT
{
class SkinPass
{
public:
    static constexpr uint32_t kMaxSets = 512;

    bool create(Context& ctx);
    void destroy(Context& ctx);
    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }

    VkDescriptorSet allocSet(Context& ctx, VkBuffer boneUbo);
    void freeSet(Context& ctx, VkDescriptorSet set);
    void dispatch(VkCommandBuffer cmd, VkDescriptorSet set, uint64_t srcAddr, uint64_t dstAddr, uint32_t count);

private:
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
}

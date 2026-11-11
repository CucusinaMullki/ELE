#pragma once
#include "RT/RTContext.h"

namespace RT
{
class ProbeUpdatePass
{
public:
    bool create(Context& ctx);
    void destroy(Context& ctx);
    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }

    bool bind(Context& ctx, VkBuffer rays, VkBuffer info, VkBuffer data);
    void record(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t probeOffset,
                uint32_t batch, uint32_t rayCount);

private:
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    bool m_bound = false;
};
}

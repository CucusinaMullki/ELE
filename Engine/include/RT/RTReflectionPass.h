#pragma once
#include "RT/RTContext.h"
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
    ASH::Texture* image(uint32_t frame) const { return frame < m_img.size() ? m_img[frame].get() : nullptr; }
    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }
    static constexpr uint32_t kMaxTextures = 1024;
    void setTextures(Context& ctx, const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal);

private:
    bool createSbt(Context& ctx);
    void createImages(Context& ctx);
    void updateSet(Context& ctx, uint32_t frame);
    void updateTextures(Context& ctx, uint32_t frame);
    
    VkSampler m_sampler = VK_NULL_HANDLE;
    std::vector<VkImageView> m_views;
    std::vector<VkImageView> m_nviews;

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_sets;

    std::vector<std::unique_ptr<ASH::Texture>> m_img;
    std::vector<ASH::Buffer*> m_cam, m_light;
    VkBuffer m_geoBuf = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;

    std::unique_ptr<ASH::Buffer> m_sbt;
    VkStridedDeviceAddressRegionKHR m_rgen{}, m_miss{}, m_hit{}, m_callable{};
    uint32_t m_frames = 0, m_w = 0, m_h = 0;
};

}
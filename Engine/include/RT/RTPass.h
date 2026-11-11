#pragma once
#include "RT/RTContext.h"
#include "RT/RTDescriptorWriter.h"
#include <ASH/ASH.h>
#include <memory>
#include <vector>

namespace RT
{

class Pass
{
public:
    static constexpr uint32_t kMaxTextures = 1024;
    using Images = std::vector<std::unique_ptr<ASH::Texture>>;

    virtual ~Pass() = default;

    static constexpr uint32_t kGBufBinding = 16;
    void setGBuffer(Pass* gbuffer) { m_gbuf = gbuffer; }

    bool create(Context& ctx, uint32_t frames, uint32_t w, uint32_t h);
    virtual void destroy(Context& ctx);
    virtual void record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame);
    virtual ASH::Texture* image(uint32_t frame) const { return out(0, frame); }

    void bindFrameBuffers(Context& ctx, uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights);
    void setTlas(Context& ctx, VkAccelerationStructureKHR tlas);
    void setGeometry(Context& ctx, ASH::Buffer* geoBuf);
    void setTextures(Context& ctx, const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal);
    void resize(Context& ctx, uint32_t w, uint32_t h);

    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }

protected:
    struct Output
    {
        uint32_t binding;
        ASH::Format format;
        const char* name;
        bool sampledAfter;
    };

    virtual const char* raygenName() const = 0;
    virtual std::vector<Output> outputs() const = 0;
    virtual void onImagesCreated(Context&) {}

    ASH::Texture* out(size_t i, uint32_t frame) const
    {
        return i < m_out.size() && frame < m_out[i].size() ? m_out[i][frame].get() : nullptr;
    }

    bool canTrace(uint32_t frame) const;

    uint32_t m_frames = 0, m_width = 0, m_height = 0;

private:
    bool createLayouts(Context& ctx);
    bool createPipelineAndSbt(Context& ctx);
    bool createSets(Context& ctx);
    void createImages(Context& ctx);
    void updateSet(Context& ctx, uint32_t frame);
    void updateAllSets(Context& ctx);

    std::vector<Output> m_outDesc;
    std::vector<Images> m_out;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_sets;

    std::unique_ptr<ASH::Buffer> m_sbt;
    VkStridedDeviceAddressRegionKHR m_rgen{}, m_miss{}, m_hit{}, m_callable{};

    std::vector<ASH::Buffer*> m_cam, m_light;
    VkAccelerationStructureKHR m_tlas = VK_NULL_HANDLE;
    VkBuffer m_geoBuf = VK_NULL_HANDLE;
    std::vector<VkImageView> m_albedo, m_normal;
    uint32_t m_frameCounter = 0;
    Pass* m_gbuf = nullptr;
};

}

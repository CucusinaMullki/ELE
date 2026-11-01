#pragma once
#include "RT/RTContext.h"
#include "RT/RTShadowPass.h"
#include "RT/RTReflection.h"
#include "RT/RTAOPass.h"
#include "RT/RTGIPass.h"
#include <ASH/ASH.h>
#include <array>
#include <vector>

namespace RT
{

class Master
{
public:
    bool init(Context* ctx, uint32_t frames, uint32_t w, uint32_t h);
    void shutdown();

    void setTlas(VkAccelerationStructureKHR tlas);
    void setGeometry(ASH::Buffer* geoBuf);
    void setTextures(const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal);
    void bindFrameBuffers(uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights);
    void resize(uint32_t w, uint32_t h);

    void recordShadows(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)     { m_shadows.record(ctx, cmd, frame); }
    void recordReflections(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame) { m_reflections.record(ctx, cmd, frame); }
    void recordAO(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)          { m_ao.record(ctx, cmd, frame); }
    void recordGI(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)          { m_gi.record(ctx, cmd, frame); }
    void recordAll(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame);
    void recordDenoise(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame, const float* view, const float* proj)
    {
        m_gi.recordDenoise(ctx, cmd, frame, view, proj);
    }

    ShadowPass& shadows() { return m_shadows; }
    ReflectionPass& reflections() { return m_reflections; }
    AOPass& ao() { return m_ao; }
    GIPass& gi() { return m_gi; }

    const ShadowPass& shadows() const { return m_shadows; }
    const ReflectionPass& reflections() const { return m_reflections; }
    const AOPass& ao() const { return m_ao; }
    const GIPass& gi() const { return m_gi; }

    ASH::Texture* shadowImage(uint32_t frame) const     { return m_shadows.image(frame); }
    ASH::Texture* reflectionImage(uint32_t frame) const { return m_reflections.image(frame); }
    ASH::Texture* aoImage(uint32_t frame) const         { return m_ao.image(frame); }
    ASH::Texture* giImage(uint32_t frame) const         { return m_gi.image(frame); }

private:
    std::array<Pass*, 4> passes() { return { &m_shadows, &m_reflections, &m_ao, &m_gi }; }

    Context* m_ctx = nullptr;
    ShadowPass m_shadows;
    ReflectionPass m_reflections;
    AOPass m_ao;
    GIPass m_gi;
};

}
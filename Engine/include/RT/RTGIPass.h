#pragma once
#include "RT/RTPass.h"
#include "RT/RTDenoiser.h"

namespace RT
{

class GIPass : public Pass
{
public:
    ASH::Texture* image(uint32_t frame) const override
    {
        return m_denoiser.ready() ? m_denoiser.outDiff() : out(0, frame);
    }
    ASH::Texture* denoisedDiffuse() const { return m_denoiser.outDiff(); }

    void recordDenoise(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame, const float* view, const float* proj);
    void destroy(Context& ctx) override;

protected:
    const char* raygenName() const override { return "gi.rgen"; }
    std::vector<Output> outputs() const override
    {
        return {
            { 1, ASH::Format::R32G32B32A32_Float, "RT GI", true  },
            { 7, ASH::Format::R32G32B32A32_Float, "RT GI nrd diff", false },
            { 8, ASH::Format::R16G16B16A16_Float, "RT GI nrd normal-roughness", false },
            { 9, ASH::Format::R32G32B32A32_Float, "RT GI nrd viewZ", false },
        };
    }
    void onImagesCreated(Context& ctx) override { m_denoiser.create(ctx, m_width, m_height, m_frames); }

private:
    Denoiser m_denoiser;
};

}
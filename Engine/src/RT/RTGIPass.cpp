#include "RT/RTGIPass.h"

namespace RT
{

void GIPass::recordDenoise(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame, const float* view, const float* proj)
{
    if (!canTrace(frame)) return;
    m_denoiser.record(ctx, cmd, frame, view, proj, out(1, frame), out(2, frame), out(3, frame));
}

void GIPass::destroy(Context& ctx)
{
    m_denoiser.destroy(ctx);
    Pass::destroy(ctx);
}

}

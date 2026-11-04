#include "RT/RTMaster.h"
#include "Core/Log.h"
#include <cstdio>

namespace RT
{

bool Master::init(Context* ctx, uint32_t frames, uint32_t w, uint32_t h)
{
    m_ctx = ctx;

    static const char* names[5] = { "gbuffer", "shadows", "reflections", "AO", "GI" };
    const auto all = passes();

    for (size_t i = 1; i < all.size(); ++i) all[i]->setGBuffer(&m_gbuffer);
    uint32_t ok = 0;
    for (size_t i = 0; i < all.size(); ++i)
    {
        const uint32_t pw = scaled(w, kScale[i]), ph = scaled(h, kScale[i]);
        if (all[i]->create(*ctx, frames, pw, ph)) { ++ok; LOG("RT Master: " << names[i] << " ok " << pw << "x" << ph << "\n"); }
        else LOG("RT Master: " << names[i] << " FAILED (disabled)\n");
    }
    createTimers(frames);
    return ok > 0;
}

void Master::shutdown()
{
    if (!m_ctx) return;
    m_ctx->ash()->waitIdle();
    for (Pass* p : passes()) p->destroy(*m_ctx);
    destroyTimers();
    m_ctx = nullptr;
}

void Master::setTlas(VkAccelerationStructureKHR tlas)
{
    if (!m_ctx) return;
    for (Pass* p : passes()) p->setTlas(*m_ctx, tlas);
}

void Master::setGeometry(ASH::Buffer* geoBuf)
{
    if (!m_ctx) return;
    for (Pass* p : passes()) p->setGeometry(*m_ctx, geoBuf);
}

void Master::setTextures(const std::vector<VkImageView>& albedo, const std::vector<VkImageView>& normal)
{
    if (!m_ctx) return;
    for (Pass* p : passes()) p->setTextures(*m_ctx, albedo, normal);
}

void Master::bindFrameBuffers(uint32_t frame, ASH::Buffer* camera, ASH::Buffer* lights)
{
    if (!m_ctx) return;
    for (Pass* p : passes()) p->bindFrameBuffers(*m_ctx, frame, camera, lights);
}

void Master::resize(uint32_t w, uint32_t h)
{
    if (!m_ctx) return;
    const auto all = passes();
    for (size_t i = 0; i < all.size(); ++i)
        all[i]->resize(*m_ctx, scaled(w, kScale[i]), scaled(h, kScale[i]));
}

void Master::recordAll(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)
{
    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());
    readTimers(frame);

    if (m_queries && frame < m_written.size())
        vkCmdResetQueryPool(vk, m_queries, frame * kStamps, kStamps);
    stamp(vk, frame, 0);

    const auto all = passes();
    for (size_t i = 0; i < all.size(); ++i)
    {
        all[i]->record(ctx, cmd, frame);
        stamp(vk, frame, uint32_t(i + 1));
    }
}

void Master::recordDenoise(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame, const float* view, const float* proj)
{
    m_gi.recordDenoise(ctx, cmd, frame, view, proj);
    stamp(reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle()), frame, kStamps - 1);
    if (frame < m_written.size()) m_written[frame] = 1;
}

void Master::createTimers(uint32_t frames)
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_ctx->physical(), &props);
    m_tickNs = props.limits.timestampPeriod;

    VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qi.queryCount = frames * kStamps;
    if (vkCreateQueryPool(m_ctx->device(), &qi, nullptr, &m_queries) != VK_SUCCESS)
    {
        m_queries = VK_NULL_HANDLE;
        LOG("RT Master: timestamp queries unavailable\n");
        return;
    }
    m_written.assign(frames, 0);
}

void Master::destroyTimers()
{
    if (m_queries && m_ctx) vkDestroyQueryPool(m_ctx->device(), m_queries, nullptr);
    m_queries = VK_NULL_HANDLE;
    m_written.clear();
    m_sumMs.fill(0.0);
    m_samples = 0;
}

void Master::stamp(VkCommandBuffer vk, uint32_t frame, uint32_t i)
{
    if (!m_queries || frame >= m_written.size()) return;
    vkCmdWriteTimestamp(vk, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queries, frame * kStamps + i);
}

void Master::readTimers(uint32_t frame)
{
    if (!m_queries || frame >= m_written.size() || !m_written[frame]) return;

    uint64_t t[kStamps]{};
    if (vkGetQueryPoolResults(m_ctx->device(), m_queries, frame * kStamps, kStamps, sizeof(t), t,
            sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) != VK_SUCCESS)
        return;

    for (uint32_t i = 0; i + 1 < kStamps; ++i)
        m_sumMs[i] += double(t[i + 1] - t[i]) * m_tickNs * 1e-6;

    if (++m_samples < 120) return;

    double total = 0.0;
    for (double v : m_sumMs) total += v;
    const double n = double(m_samples);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "RT GPU ms: gbuf %.2f | shadows %.2f | refl %.2f | ao %.2f | gi %.2f | nrd %.2f | total %.2f\n",
        m_sumMs[0] / n, m_sumMs[1] / n, m_sumMs[2] / n, m_sumMs[3] / n, m_sumMs[4] / n, m_sumMs[5] / n, total / n);
    LOG(buf);
    m_sumMs.fill(0.0);
    m_samples = 0;
}

}
#include "RT/RTMaster.h"
#include "Core/Log.h"

namespace RT
{

bool Master::init(Context* ctx, uint32_t frames, uint32_t w, uint32_t h)
{
    m_ctx = ctx;

    // Проход, который не создался (например, нет шейдера), просто выключен:
    // его image() вернёт nullptr, и материал получит заглушку. RT целиком не падает.
    struct Entry { Pass* pass; const char* name; };
    const Entry entries[] = {
        { &m_shadows,     "shadows"     },
        { &m_reflections, "reflections" },
        { &m_ao,          "AO"          },
        { &m_gi,          "GI"          },
    };

    uint32_t ok = 0;
    for (const auto& e : entries)
    {
        if (e.pass->create(*ctx, frames, w, h)) { ++ok; LOG("RT Master: " << e.name << " ok\n"); }
        else LOG("RT Master: " << e.name << " FAILED (disabled)\n");
    }
    return ok > 0;
}

void Master::shutdown()
{
    if (!m_ctx) return;
    m_ctx->ash()->waitIdle();
    for (Pass* p : passes()) p->destroy(*m_ctx);
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
    for (Pass* p : passes()) p->resize(*m_ctx, w, h);
}

void Master::recordAll(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame)
{
    for (Pass* p : passes()) p->record(ctx, cmd, frame);
}

}
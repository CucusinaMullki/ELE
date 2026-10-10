#include "Vulkan/Particles.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace
{
std::vector<unsigned char> makeSoftCircle(int n)
{
    std::vector<unsigned char> px((size_t)n * n * 4);
    for (int y = 0; y < n; ++y)
    {
        for (int x = 0; x < n; ++x)
        {
            float dx = ((float)x + 0.5f) / n * 2.0f - 1.0f;
            float dy = ((float)y + 0.5f) / n * 2.0f - 1.0f;
            float r = std::sqrt(dx * dx + dy * dy);
            float a = std::clamp(1.0f - r, 0.0f, 1.0f);
            a = a * a * (3.0f - 2.0f * a);
            unsigned char* p = &px[((size_t)y * n + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(a * 255.0f + 0.5f);
        }
    }
    return px;
}
}

void Particles::initialize(ASH::Device* device, ASH::RenderPass* renderPass, uint32_t frameCount, ASH::Texture* depthTexture, ASH::Sampler* depthSampler, const DecalCameraBuffer& cameraBuffer)
{
    m_device = device;
    m_renderPass = renderPass;
    m_frameCount = frameCount;
    m_depthTexture = depthTexture;
    m_depthSampler = depthSampler;
    m_camera = &cameraBuffer;

    ComputePipelineConfig cc{};
    cc.computeShader = "particles";
    cc.layoutType = LayoutType::ParticleCompute;
    cc.pushConstantSize = sizeof(ComputePush);
    cc.pushStage = ASH::ShaderStage::Compute;
    m_compute.initialize(device, cc);

    PipelineConfig dc{};
    dc.vertexShader = "particles";
    dc.fragmentShader = "particles";
    dc.renderPass = renderPass;
    dc.frameCount = frameCount;
    dc.layoutType = LayoutType::ParticelDraw;
    dc.colorAttachmentCount = 1;
    dc.useVertexInput = false;
    dc.depthTest = false;
    dc.depthWrite = false;
    dc.cullMode = ASH::CullMode::None;
    dc.blendEnable = true;
    dc.blendOp = ASH::BlendOp::Add;
    dc.blendSrcFactor = ASH::BlendFactor::SrcAlpha;
    dc.pushConstantSize = sizeof(DrawPush);
    dc.pushStage = ASH::ShaderStage::Vertex;

    dc.blendDstFactor = ASH::BlendFactor::OneMinusSrcAlpha;
    m_drawAlpha.initialize(device, dc);

    dc.blendDstFactor = ASH::BlendFactor::One;
    m_drawAdditive.initialize(device, dc);

    auto px = makeSoftCircle(64);
    m_defaultSprite.initializeFromMemory(device, px.data(), 64, 64, false);
}

void Particles::cleanup()
{
    unload();
    m_sprites.clear();
    m_defaultSprite.cleanup();
    m_drawAdditive.cleanup();
    m_drawAlpha.cleanup();
    m_compute.cleanup();
}
 
void Particles::unload()
{
    if (m_device) m_device->waitIdle();
    m_drawSets.clear();
    m_computeSets.clear();
    m_outBuffers.clear();
    m_effectBuffer.reset();
    m_emitterSprite.clear();
    for (auto& kv : m_sprites) kv.second->cleanup();
    m_sprites.clear();
    m_emitters.clear();
    m_records.clear();
    m_totalSlots = 0;
}

Texture* Particles::loadSprite(const std::string& rawName, const std::string& baseDir)
{
    if (rawName.empty()) return &m_defaultSprite;
 
    std::filesystem::path p = Paths::resolveAsset(rawName, baseDir);
    if (!std::filesystem::exists(p))
    {
        LOG("[Particles] texture not found: " << rawName << " (using default sprite)");
        return &m_defaultSprite;
    }
 
    const std::string key = p.string();
    auto it = m_sprites.find(key);
    if (it != m_sprites.end()) return it->second.get();
 
    auto tex = std::make_unique<Texture>();
    tex->initialize(m_device, key, true);
    Texture* raw = tex.get();
    m_sprites[key] = std::move(tex);
    return raw;
}
 
bool Particles::load(const std::string& path)
{
    unload();
 
    std::string err;
    if (!ParticlesLoader::load(path, m_records, &err))
    {
        LOG("[Particles] cannot load " << path << ": " << err);
        m_records.clear();
        return false;
    }
    if (m_records.empty()) return true;
 
    const std::string baseDir = std::filesystem::path(path).parent_path().string();
 
    uint32_t base = 0;
    for (uint32_t i = 0; i < (uint32_t)m_records.size(); ++i)
    {
        const auto& r = m_records[i];
        Emitter e{};
        e.name = r.name;
        e.effect = i;
        e.outBase = base;
        e.count = r.particleCount;
        e.additive = (r.flags & PFLAG_ADDITIVE) != 0;
        m_emitters.push_back(e);
        base += r.particleCount;
    }
    m_totalSlots = base;
 
    {
        ASH::BufferDesc d{};
        d.size = sizeof(ParticleEffectRecord) * m_records.size();
        d.usage = ASH::BufferUsage::StorageBuffer;
        d.memory = ASH::MemoryUsage::CpuToGpu;
        m_effectBuffer = m_device->createBuffer(d);
        void* mapped = m_effectBuffer->map();
        std::memcpy(mapped, m_records.data(), d.size);
        m_effectBuffer->unmap();
    }
 
    m_outBuffers.resize(m_frameCount);
    for (uint32_t f = 0; f < m_frameCount; ++f)
    {
        ASH::BufferDesc d{};
        d.size = (size_t)m_totalSlots * kInstanceStride;
        d.usage = ASH::BufferUsage::StorageBuffer;
        d.memory = ASH::MemoryUsage::CpuToGpu;
        m_outBuffers[f] = m_device->createBuffer(d);
        void* mapped = m_outBuffers[f]->map();
        std::memset(mapped, 0, d.size);
        m_outBuffers[f]->unmap();
    }
 
    for (const auto& r : m_records)
        m_emitterSprite.push_back(loadSprite(r.texture, baseDir));
 
    createDescriptorSets();
 
    LOG("[Particles] " << path << ": " << m_records.size() << " effects, " << m_totalSlots << " particle slots");
    return true;
}
 
void Particles::createDescriptorSets()
{
    m_computeSets.clear();
    m_drawSets.clear();
    if (!m_effectBuffer) return;
 
    ASH::DescriptorBufferInfo effInfo{ m_effectBuffer.get(), 0, m_effectBuffer->getSize() };
 
    m_computeSets.resize(m_frameCount);
    for (uint32_t f = 0; f < m_frameCount; ++f)
    {
        m_computeSets[f] = m_compute.createDescriptorSet();
        ASH::DescriptorBufferInfo outInfo{ m_outBuffers[f].get(), 0, m_outBuffers[f]->getSize() };
 
        std::array<ASH::DescriptorWrite, 2> w{};
        w[0] = { 0, ASH::DescriptorType::StorageBuffer, 0, &effInfo, nullptr };
        w[1] = { 1, ASH::DescriptorType::StorageBuffer, 0, &outInfo, nullptr };
        m_computeSets[f]->update(w.data(), (uint32_t)w.size());
    }
 
    m_drawSets.resize(m_emitters.size());
    for (size_t e = 0; e < m_emitters.size(); ++e)
    {
        m_drawSets[e].resize(m_frameCount);
        for (uint32_t f = 0; f < m_frameCount; ++f)
        {
            m_drawSets[e][f] = m_drawAlpha.createDescriptorSet();
 
            Texture* sprite = m_emitterSprite[e];
            ASH::DescriptorImageInfo spriteInfo{ sprite->getTexture(), sprite->getSampler() };
            ASH::DescriptorImageInfo depthInfo{ m_depthTexture, m_depthSampler };
            ASH::DescriptorBufferInfo camInfo{ m_camera->getBuffer(f), 0, m_camera->getBuffer(f)->getSize() };
            ASH::DescriptorBufferInfo outInfo{ m_outBuffers[f].get(), 0, m_outBuffers[f]->getSize() };
 
            std::array<ASH::DescriptorWrite, 5> w{};
            w[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &spriteInfo };
            w[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &depthInfo };
            w[2] = { 2, ASH::DescriptorType::UniformBuffer, 0, &camInfo, nullptr };
            w[3] = { 3, ASH::DescriptorType::StorageBuffer, 0, &outInfo, nullptr };
            w[4] = { 4, ASH::DescriptorType::StorageBuffer, 0, &effInfo, nullptr };
            m_drawSets[e][f]->update(w.data(), (uint32_t)w.size());
        }
    }
}
 
void Particles::rebuildDescriptors(ASH::Texture* depthTexture, ASH::Sampler* depthSampler)
{
    m_depthTexture = depthTexture;
    m_depthSampler = depthSampler;
    if (m_device) m_device->waitIdle();
    createDescriptorSets();
}
 
bool Particles::hasWork() const
{
    if (!m_effectBuffer) return false;
    for (const auto& e : m_emitters)
        if (e.active) return true;
    return false;
}
 
int Particles::findEmitter(const std::string& name) const
{
    for (size_t i = 0; i < m_emitters.size(); ++i)
        if (m_emitters[i].name == name) return (int)i;
    return -1;
}
 
void Particles::recordCompute(ASH::CommandBuffer* cmd, uint32_t frameIndex, float totalTime)
{
    m_time = totalTime;
    if (!hasWork()) return;
 
    ASH::Pipeline* pipe = m_compute.getPipeline();
    cmd->bindPipeline(pipe);
    cmd->bindDescriptorSet(pipe, 0, m_computeSets[frameIndex].get());
 
    for (const auto& e : m_emitters)
    {
        if (!e.active) continue;
        ComputePush pc{};
        pc.offsetTime = glm::vec4(e.offset, totalTime - e.startTime);
        pc.info[0] = e.effect;
        pc.info[1] = e.outBase;
        cmd->pushConstants(pipe, ASH::ShaderStage::Compute, 0, sizeof(pc), &pc);
        cmd->dispatch((e.count + 63u) / 64u, 1, 1);
    }
 
    ASH::BufferBarrier bb{};
    bb.buffer = m_outBuffers[frameIndex].get();
    cmd->barrier(nullptr, 0, &bb, 1);
}
 
void Particles::recordDraw(ASH::CommandBuffer* cmd, uint32_t frameIndex)
{
    if (!hasWork()) return;
 
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool additive = (pass == 1);
        ASH::Pipeline* pipe = additive ? m_drawAdditive.getPipeline() : m_drawAlpha.getPipeline();
        bool bound = false;
 
        for (size_t i = 0; i < m_emitters.size(); ++i)
        {
            const Emitter& e = m_emitters[i];
            if (!e.active || e.additive != additive) continue;
 
            if (!bound) { cmd->bindPipeline(pipe); bound = true; }
            cmd->bindDescriptorSet(pipe, 0, m_drawSets[i][frameIndex].get());
 
            DrawPush dp{};
            dp.info[0] = e.effect;
            dp.info[1] = e.outBase;
            cmd->pushConstants(pipe, ASH::ShaderStage::Vertex, 0, sizeof(dp), &dp);
            cmd->draw(e.count * 6u, 1, 0, 0);
        }
    }
}
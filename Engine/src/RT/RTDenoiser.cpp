#include "RT/RTDenoiser.h"

#ifdef ELE_HAS_NRD
#include <cstring>
#include <ASH/CommandBuffer.h>
#include <NRD.h>
#include <iostream>

namespace RT
{
namespace
{
ASH::Format toAsh(nrd::Format f)
{
    switch (f)
    {
        case nrd::Format::R8_UNORM: return ASH::Format::R8_UNorm;
        case nrd::Format::R8_UINT: return ASH::Format::R8_UInt;
        case nrd::Format::R16_UINT: return ASH::Format::R16_UInt;
        case nrd::Format::R16_SFLOAT: return ASH::Format::R16_Float;
        case nrd::Format::R32_SFLOAT: return ASH::Format::R32_Float;
        case nrd::Format::RGBA16_SFLOAT: return ASH::Format::R16G16B16A16_Float;
        case nrd::Format::R10_G10_B10_A2_UNORM: return ASH::Format::R10G10B10A2_UNorm;
        default: return ASH::Format::Unknown;
    }
}
}

bool Denoiser::create(Context& ctx, uint32_t w, uint32_t h, uint32_t frames)
{
    destroy(ctx);
    VkDevice dev = ctx.device();
    m_w = w; m_h = h; m_frames = frames;

    auto fail = [&](const char* what) {
        std::cout << "NRD: create failed: " << what << "\n";
        destroy(ctx);
        return false;
    };

    nrd::DenoiserDesc dd{};
    dd.identifier = 0;
    dd.denoiser = nrd::Denoiser::REBLUR_DIFFUSE;
    nrd::InstanceCreationDesc cd{};
    cd.denoisers = &dd;
    cd.denoisersNum = 1;
    if (nrd::CreateInstance(cd, m_inst) != nrd::Result::SUCCESS || !m_inst)
        return fail("CreateInstance");

    const nrd::LibraryDesc& lib = *nrd::GetLibraryDesc();
    const nrd::InstanceDesc& d = *nrd::GetInstanceDesc(*m_inst);

    if (d.resourcesSpaceIndex != 0 || d.constantBufferAndSamplersSpaceIndex != 1)
        return fail("unexpected descriptor space indices");

    auto makeTex = [&](const auto& t, const char* name) -> std::unique_ptr<ASH::Texture> {
        ASH::TextureDesc td{};
        td.type = ASH::TextureType::Texture2D;
        td.format = toAsh(t.format);
        if (td.format == ASH::Format::Unknown) return nullptr;
        const uint32_t df = t.downsampleFactor ? t.downsampleFactor : 1;
        td.extent = { (m_w + df - 1) / df, (m_h + df - 1) / df, 1 };
        td.mipLevels = 1; td.arrayLayers = 1; td.sampleCount = 1;
        td.usage = ASH::TextureUsage::Storage | ASH::TextureUsage::Sampled;
        td.debugName = name;
        return ctx.ash()->createTexture(td);
    };
    for (uint32_t i = 0; i < d.permanentPoolSize; ++i)
    {
        auto t = makeTex(d.permanentPool[i], "NRD permanent");
        if (!t) return fail("unsupported format in permanent pool");
        m_perm.push_back(std::move(t));
    }
    for (uint32_t i = 0; i < d.transientPoolSize; ++i)
    {
        auto t = makeTex(d.transientPool[i], "NRD transient");
        if (!t) return fail("unsupported format in transient pool");
        m_trans.push_back(std::move(t));
    }

    {
        ASH::TextureDesc od{};
        od.type = ASH::TextureType::Texture2D;
        od.format = ASH::Format::R16G16B16A16_Float;
        od.extent = { m_w, m_h, 1 };
        od.mipLevels = 1; od.arrayLayers = 1; od.sampleCount = 1;
        od.usage = ASH::TextureUsage::Storage | ASH::TextureUsage::Sampled;
        od.debugName = "NRD out diffuse";
        m_out = ctx.ash()->createTexture(od);
        if (!m_out) return fail("out texture");
    }
    m_needInit = true; m_hasPrev = false; m_frameIndex = 0;

    m_samplersNum = d.samplersNum > 2 ? 2 : d.samplersNum;
    for (uint32_t i = 0; i < m_samplersNum; ++i)
    {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        const bool nearest = d.samplers[i] == nrd::Sampler::NEAREST_CLAMP;
        si.magFilter = si.minFilter = nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.maxLod = 0.0f;
        if (vkCreateSampler(dev, &si, nullptr, &m_samplers[i]) != VK_SUCCESS) return fail("vkCreateSampler");
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> bs;
        for (uint32_t i = 0; i < m_samplersNum; ++i)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = lib.spirvBindingOffsets.samplerOffset + d.samplersBaseRegisterIndex + i;
            b.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            b.pImmutableSamplers = &m_samplers[i];
            bs.push_back(b);
        }
        VkDescriptorSetLayoutBinding cb{};
        cb.binding = lib.spirvBindingOffsets.constantBufferOffset + d.constantBufferRegisterIndex;
        cb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        cb.descriptorCount = 1;
        cb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bs.push_back(cb);

        VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        lci.bindingCount = uint32_t(bs.size());
        lci.pBindings = bs.data();
        if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &m_cbLayout) != VK_SUCCESS) return fail("cb layout");

        const VkDescriptorPoolSize ps[2] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, m_samplersNum },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 } };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = 1;
        pci.poolSizeCount = 2;
        pci.pPoolSizes = ps;
        if (vkCreateDescriptorPool(dev, &pci, nullptr, &m_cbPool) != VK_SUCCESS) return fail("cb pool");

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = m_cbPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_cbLayout;
        if (vkAllocateDescriptorSets(dev, &ai, &m_cbSet) != VK_SUCCESS) return fail("cb set");

        m_slots = d.descriptorPoolDesc.setsMaxNum;
        m_cbStride = (d.constantBufferMaxDataSize + 255u) & ~255u;
        m_cbBinding = cb.binding;
        m_cb = ctx.createBuffer(size_t(m_cbStride) * m_slots * frames,
            ASH::BufferUsage::UniformBuffer, ASH::MemoryUsage::CpuToGpu, "NRD constants");
        if (!m_cb) return fail("cb buffer");
        VkDescriptorBufferInfo cbi{ reinterpret_cast<VkBuffer>(m_cb->getNativeHandle()), 0,
            d.constantBufferMaxDataSize };
        VkWriteDescriptorSet cw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        cw.dstSet = m_cbSet;
        cw.dstBinding = cb.binding;
        cw.descriptorCount = 1;
        cw.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        cw.pBufferInfo = &cbi;
        vkUpdateDescriptorSets(dev, 1, &cw, 0, nullptr);
    }

    for (uint32_t p = 0; p < d.pipelinesNum; ++p)
    {
        const nrd::PipelineDesc& pd = d.pipelines[p];

        std::vector<VkDescriptorSetLayoutBinding> bs;
        uint32_t texReg = 0, uavReg = 0;
        for (uint32_t r = 0; r < pd.resourceRangesNum; ++r)
        {
            const auto& rg = pd.resourceRanges[r];
            const bool tex = rg.descriptorType == nrd::DescriptorType::TEXTURE;
            const uint32_t off = tex ? lib.spirvBindingOffsets.textureOffset
                                     : lib.spirvBindingOffsets.storageTextureAndBufferOffset;
            for (uint32_t i = 0; i < rg.descriptorsNum; ++i)
            {
                VkDescriptorSetLayoutBinding b{};
                b.binding = off + d.resourcesBaseRegisterIndex + (tex ? texReg++ : uavReg++);
                b.descriptorType = tex ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                b.descriptorCount = 1;
                b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                bs.push_back(b);
            }
        }
        VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        lci.bindingCount = uint32_t(bs.size());
        lci.pBindings = bs.data();
        VkDescriptorSetLayout resLayout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &resLayout) != VK_SUCCESS) return fail("res layout");
        m_resLayouts.push_back(resLayout);

        const VkDescriptorSetLayout sets[2] = { resLayout, m_cbLayout };
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 2;
        plci.pSetLayouts = sets;
        VkPipelineLayout pl = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(dev, &plci, nullptr, &pl) != VK_SUCCESS) return fail("pipeline layout");
        m_pipeLayouts.push_back(pl);

        VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        smci.codeSize = size_t(pd.computeShaderSPIRV.size);
        smci.pCode = static_cast<const uint32_t*>(pd.computeShaderSPIRV.bytecode);
        VkShaderModule mod = VK_NULL_HANDLE;
        if (vkCreateShaderModule(dev, &smci, nullptr, &mod) != VK_SUCCESS) return fail("shader module");

        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
            VK_SHADER_STAGE_COMPUTE_BIT, mod, d.shaderEntryPoint, nullptr};
        ci.layout = pl;
        VkPipeline pipe = VK_NULL_HANDLE;
        const VkResult rr = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe);
        vkDestroyShaderModule(dev, mod, nullptr);
        if (rr != VK_SUCCESS) return fail("compute pipeline");
        m_pipelines.push_back(pipe);
    }

    for (uint32_t f = 0; f < frames; ++f)
    {
        const VkDescriptorPoolSize ps[2] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, d.descriptorPoolDesc.totalTexturesNum },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, d.descriptorPoolDesc.totalStorageTexturesNum } };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = d.descriptorPoolDesc.setsMaxNum;
        pci.poolSizeCount = 2;
        pci.pPoolSizes = ps;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(dev, &pci, nullptr, &pool) != VK_SUCCESS) return fail("frame pool");
        m_pools.push_back(pool);
    }

    m_ready = true;
    std::cout << "NRD: denoiser ready (" << m_pipelines.size() << " pipelines, "
              << (m_perm.size() + m_trans.size()) << " textures)\n";
    return true;
}

void Denoiser::destroy(Context& ctx)
{
    VkDevice dev = ctx.device();
    for (auto p : m_pools) if (p) vkDestroyDescriptorPool(dev, p, nullptr);
    for (auto p : m_pipelines) if (p) vkDestroyPipeline(dev, p, nullptr);
    for (auto p : m_pipeLayouts) if (p) vkDestroyPipelineLayout(dev, p, nullptr);
    for (auto l : m_resLayouts) if (l) vkDestroyDescriptorSetLayout(dev, l, nullptr);
    m_pools.clear(); m_pipelines.clear(); m_pipeLayouts.clear(); m_resLayouts.clear();

    if (m_cbPool) vkDestroyDescriptorPool(dev, m_cbPool, nullptr);
    if (m_cbLayout) vkDestroyDescriptorSetLayout(dev, m_cbLayout, nullptr);
    m_cbPool = VK_NULL_HANDLE; m_cbLayout = VK_NULL_HANDLE; m_cbSet = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < 2; ++i)
    {
        if (m_samplers[i]) vkDestroySampler(dev, m_samplers[i], nullptr);
        m_samplers[i] = VK_NULL_HANDLE;
    }
    m_samplersNum = 0;

    m_out.reset();
    m_cb.reset();
    m_perm.clear();
    m_trans.clear();

    if (m_inst) nrd::DestroyInstance(*m_inst);
    m_inst = nullptr;
    m_ready = false;
}

namespace
{
void fullBarrierDn(VkCommandBuffer vk)
{
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &mb, 0, nullptr, 0, nullptr);
}
}

void Denoiser::record(Context& ctx, ASH::CommandBuffer* cmd, uint32_t frame,
    const float* view, const float* proj,
    ASH::Texture* inDiff, ASH::Texture* inNr, ASH::Texture* inVz)
{
    if (!m_ready || !m_inst || frame >= m_frames || !inDiff || !inNr || !inVz) return;

    VkDevice dev = ctx.device();
    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());
    const nrd::LibraryDesc& lib = *nrd::GetLibraryDesc();
    const nrd::InstanceDesc& d = *nrd::GetInstanceDesc(*m_inst);

    if (m_needInit)
    {
        auto toGeneral = [&](ASH::Texture* t) {
            ASH::TextureBarrier tb{};
            tb.texture = t;
            tb.oldState = ASH::ResourceState::Undefined;
            tb.newState = ASH::ResourceState::General;
            cmd->barrier(&tb, 1, nullptr, 0);
        };
        for (auto& t : m_perm) toGeneral(t.get());
        for (auto& t : m_trans) toGeneral(t.get());
        toGeneral(m_out.get());
        m_needInit = false;
    }
    else
    {
        ASH::TextureBarrier tb{};
        tb.texture = m_out.get();
        tb.oldState = ASH::ResourceState::ShaderReadOnly;
        tb.newState = ASH::ResourceState::General;
        cmd->barrier(&tb, 1, nullptr, 0);
    }
    fullBarrierDn(vk);

    nrd::CommonSettings cs{};
    const float* pv = m_hasPrev ? m_prevView : view;
    const float* pp = m_hasPrev ? m_prevProj : proj;
    std::memcpy(cs.viewToClipMatrix, proj, sizeof(float) * 16);
    std::memcpy(cs.viewToClipMatrixPrev, pp, sizeof(float) * 16);
    std::memcpy(cs.worldToViewMatrix, view, sizeof(float) * 16);
    std::memcpy(cs.worldToViewMatrixPrev, pv, sizeof(float) * 16);
    cs.motionVectorScale[0] = cs.motionVectorScale[1] = cs.motionVectorScale[2] = 0.0f;
    cs.isMotionVectorInWorldSpace = true;
    cs.resourceSize[0] = cs.resourceSizePrev[0] = cs.rectSize[0] = cs.rectSizePrev[0] = uint16_t(m_w);
    cs.resourceSize[1] = cs.resourceSizePrev[1] = cs.rectSize[1] = cs.rectSizePrev[1] = uint16_t(m_h);
    cs.frameIndex = m_frameIndex++;
    cs.accumulationMode = m_hasPrev ? nrd::AccumulationMode::CONTINUE : nrd::AccumulationMode::RESTART;
    if (nrd::SetCommonSettings(*m_inst, cs) != nrd::Result::SUCCESS) return;

    nrd::ReblurSettings rs{};
    if (nrd::SetDenoiserSettings(*m_inst, 0, &rs) != nrd::Result::SUCCESS) return;

    const nrd::Identifier id = 0;
    const nrd::DispatchDesc* dispatches = nullptr;
    uint32_t dispatchesNum = 0;
    if (nrd::GetComputeDispatches(*m_inst, &id, 1, dispatches, dispatchesNum) != nrd::Result::SUCCESS) return;
    if (dispatchesNum > m_slots) { std::cout << "NRD: too many dispatches\n"; return; }

    vkResetDescriptorPool(dev, m_pools[frame], 0);

    auto texFor = [&](const nrd::ResourceDesc& r) -> ASH::Texture* {
        switch (r.type)
        {
            case nrd::ResourceType::IN_MV: return inNr;
            case nrd::ResourceType::IN_NORMAL_ROUGHNESS: return inNr;
            case nrd::ResourceType::IN_VIEWZ: return inVz;
            case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST: return inDiff;
            case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST: return m_out.get();
            case nrd::ResourceType::TRANSIENT_POOL:
                return r.indexInPool < m_trans.size() ? m_trans[r.indexInPool].get() : nullptr;
            case nrd::ResourceType::PERMANENT_POOL:
                return r.indexInPool < m_perm.size() ? m_perm[r.indexInPool].get() : nullptr;
            default: return nullptr;
        }
    };

    uint8_t* cbMem = static_cast<uint8_t*>(m_cb->map());
    uint32_t lastOffset = 0;

    for (uint32_t i = 0; i < dispatchesNum; ++i)
    {
        const nrd::DispatchDesc& dd = dispatches[i];
        if (dd.pipelineIndex >= m_pipelines.size()) continue;

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = m_pools[frame];
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &m_resLayouts[dd.pipelineIndex];
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(dev, &ai, &set) != VK_SUCCESS) { std::cout << "NRD: set alloc failed\n"; break; }

        std::vector<VkDescriptorImageInfo> infos(dd.resourcesNum);
        std::vector<VkWriteDescriptorSet> writes(dd.resourcesNum);
        uint32_t texReg = 0, uavReg = 0;
        bool ok = true;
        for (uint32_t r = 0; r < dd.resourcesNum; ++r)
        {
            const nrd::ResourceDesc& rd = dd.resources[r];
            ASH::Texture* t = texFor(rd);
            if (!t) { ok = false; break; }
            const bool tex = rd.descriptorType == nrd::DescriptorType::TEXTURE;
            const uint32_t off = tex ? lib.spirvBindingOffsets.textureOffset
                                     : lib.spirvBindingOffsets.storageTextureAndBufferOffset;
            infos[r] = { VK_NULL_HANDLE, reinterpret_cast<VkImageView>(t->getFaceView(0, 0)), VK_IMAGE_LAYOUT_GENERAL };
            writes[r] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[r].dstSet = set;
            writes[r].dstBinding = off + d.resourcesBaseRegisterIndex + (tex ? texReg++ : uavReg++);
            writes[r].descriptorCount = 1;
            writes[r].descriptorType = tex ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[r].pImageInfo = &infos[r];
        }
        if (!ok) { std::cout << "NRD: unmapped resource in dispatch " << i << "\n"; continue; }
        vkUpdateDescriptorSets(dev, uint32_t(writes.size()), writes.data(), 0, nullptr);

        uint32_t offset = lastOffset;
        if (dd.constantBufferData && dd.constantBufferDataSize > 0 && dd.constantBufferDataSize <= d.constantBufferMaxDataSize)
        {
            offset = (frame * m_slots + i) * m_cbStride;
            std::memcpy(cbMem + offset, dd.constantBufferData, dd.constantBufferDataSize);
            lastOffset = offset;
        }

        const VkDescriptorSet sets[2] = { set, m_cbSet };
        vkCmdBindPipeline(vk, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[dd.pipelineIndex]);
        vkCmdBindDescriptorSets(vk, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayouts[dd.pipelineIndex],
            0, 2, sets, 1, &offset);
        vkCmdDispatch(vk, dd.gridWidth, dd.gridHeight, 1);
        fullBarrierDn(vk);
    }
    m_cb->unmap();

    {
        ASH::TextureBarrier tb{};
        tb.texture = m_out.get();
        tb.oldState = ASH::ResourceState::General;
        tb.newState = ASH::ResourceState::ShaderReadOnly;
        cmd->barrier(&tb, 1, nullptr, 0);
    }

    std::memcpy(m_prevView, view, sizeof(float) * 16);
    std::memcpy(m_prevProj, proj, sizeof(float) * 16);
    m_hasPrev = true;
}

}

#else

namespace RT
{
bool Denoiser::create(Context&, uint32_t, uint32_t, uint32_t) { return false; }
void Denoiser::destroy(Context&) {}
}

#endif

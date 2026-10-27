#include "RT/RTWorld.h"
#include "Core/Log.h"
#include "Components/RigidBodyComponent.h"
#include <algorithm>
#include <cstring>
#include "unordered_set"

namespace RT {

bool World::init(ASH::Device* device)
{
    m_ready = m_ctx.init(device);
    if (m_ready && !m_shadows.createPipeline(m_ctx)) {
        LOG("RT: shadow pipeline FAILED\n");
        m_shadows.destroy(m_ctx);
        m_ready = false;
    }
    else if (m_ready) LOG("RT: shadow pipeline ok\n");

    if (m_ready) {
        if (m_reflections.createPipeline(m_ctx)) LOG("RT: reflection pipeline ok\n");
        else { LOG("RT: reflection pipeline FAILED\n"); m_reflections.destroy(m_ctx); }
        if (m_reflections.createProbePipeline(m_ctx)) LOG("RT: probe pipeline ok\n");
        else LOG("RT: probe pipeline FAILED\n");
        if (m_skin.create(m_ctx)) LOG("RT: skin pipeline ok\n");
        else { LOG("RT: skin pipeline FAILED\n"); m_skin.destroy(m_ctx); }
    }
    return m_ready;
}

void World::destroyTlas()
{
    m_shadows.setTlas(m_ctx, VK_NULL_HANDLE);
    m_reflections.setTlas(m_ctx, VK_NULL_HANDLE);
    m_reflections.setGeometry(m_ctx, nullptr);
    if (m_tlas) m_ctx.vkDestroyAS(m_ctx.device(), m_tlas, nullptr);
    m_tlas = VK_NULL_HANDLE;
    m_geoTable.reset();
    m_geoCount = 0;
    m_tlasStorage.reset();
    m_instanceCount = 0;
    for (auto& b : m_instBuf) b.reset();
    m_tlasScratch.reset();
    m_scratchAddr = 0;
    m_dyn.clear();
}

void World::shutdown()
{
    if (!m_ready) return;
    m_ctx.ash()->waitIdle();
    destroyTlas();
    for (auto& kv : m_skinned) kv.second->blas.destroy(m_ctx);
    m_skinned.clear();
    m_skin.destroy(m_ctx);
    for (auto& kv : m_blas) kv.second->destroy(m_ctx);
    m_skinned.clear();
    m_skin.destroy(m_ctx);
    m_blas.clear();
    m_shadows.destroy(m_ctx);
    m_reflections.destroy(m_ctx);
    m_albedoViews.clear();
    m_ctx.shutdown();
    m_ready = false;
    m_matSlots.clear();
    m_reflections.setTextures(m_ctx, m_albedoViews, m_normalViews);
    destroyTlas();
}

VkImageView World::normalView(ASH::Texture* t)
{
    return reinterpret_cast<VkImageView>(t->getFaceView(0, 0));
}

bool World::addMesh(const void* key, const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices, const std::vector<Submesh>& submeshes)
{
    if (!m_ready || vertices.empty() || indices.size() < 3) return false;

    std::vector<BlasRange> ranges;
    for (const auto& sm : submeshes)
        ranges.push_back({ (uint32_t)sm.firstIndex, (uint32_t)sm.indexCount });

    BlasInput in;
    in.vertices = vertices.data();
    in.vertexCount = (uint32_t)vertices.size();
    in.vertexStride = sizeof(Vertex);
    in.indices = indices.data();
    in.indexCount = (uint32_t)indices.size();
    in.ranges = ranges.empty() ? nullptr : ranges.data();
    in.rangeCount = (uint32_t)ranges.size();

    auto blas = std::make_unique<Blas>();
    if (!blas->build(m_ctx, in)) return false;
    m_blas[key] = std::move(blas);
    return true;
}

void World::updateDynamic(const ::Scene& scene, ASH::CommandBuffer* cmd, uint32_t frame)
{
    if (!m_ready || !m_tlas || m_dyn.empty() || frame >= kFrames || !m_instBuf[frame]) return;
    if (scene.getVersion() != m_syncedVersion) return;

    const auto& objs = scene.getObjects();
    auto* dst = static_cast<VkAccelerationStructureInstanceKHR*>(m_instBuf[frame]->map());
    for (const auto& d : m_dyn) {
        if (d.obj >= objs.size()) continue;
        const glm::mat4 m = objs[d.obj]->transform.getMatrix();
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                dst[d.inst].transform.matrix[r][c] = m[c][r];
    }

    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());

    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(vk,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &mb, 0, nullptr, 0, nullptr);

    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geo.flags = 0;
    geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geo.geometry.instances.arrayOfPointers = VK_FALSE;
    geo.geometry.instances.data.deviceAddress = m_instBuf[frame]->getDeviceAddress();

    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.geometryCount = 1;
    bi.pGeometries = &geo;
    bi.dstAccelerationStructure = m_tlas;
    bi.scratchData.deviceAddress = m_scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = m_instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    m_ctx.vkCmdBuildAS(vk, 1, &bi, &pRange);

    mb.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(vk, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

static VkBuffer boneVk(SkeletalMeshComponent& sk, uint32_t frame)
{
    ASH::Buffer* b = sk.getBoneBuffer(frame);
    return b ? reinterpret_cast<VkBuffer>(b->getNativeHandle()) : VK_NULL_HANDLE;
}

World::SkinInst* World::getOrCreateSkinned(GameObject& o, SkeletalMeshComponent& sk)
{
    auto it = m_skinned.find(&o);
    if (it != m_skinned.end()) return it->second->blas.valid() ? it->second.get() : nullptr;

    auto mesh = sk.getSkeletalMesh();
    if (!m_skin.ready() || !mesh) return nullptr;
    const auto& v = mesh->getCpuVertices();
    const auto& idx = mesh->getCpuIndices();
    if (v.empty() || idx.size() < 3) return nullptr;

    std::vector<BlasRange> ranges;
    for (const auto& sm : mesh->getSubmeshes())
        ranges.push_back({ (uint32_t)sm.firstIndex, (uint32_t)sm.indexCount });

    auto si = std::make_unique<SkinInst>();
    SkinInst* raw = si.get();
    const bool ok = si->blas.create(m_ctx, v.data(), (uint32_t)v.size(), idx.data(), (uint32_t)idx.size(),
        ranges.empty() ? nullptr : ranges.data(), (uint32_t)ranges.size());
    m_skinned[&o] = std::move(si);
    if (!ok) { LOG("RT: skinned BLAS create FAILED\n"); return nullptr; }

    m_ctx.ash()->waitIdle();
    sk.uploadBoneMatrices(0);
    raw->sets[0] = m_skin.allocSet(m_ctx, boneVk(sk, 0));
    if (!raw->sets[0]) return nullptr;
    m_ctx.submitOneTime([&](VkCommandBuffer c) { raw->blas.record(m_ctx, m_skin, c, raw->sets[0]); });
    LOG("RT: skinned BLAS created\n");
    return raw;
}

void World::updateSkinned(const ::Scene& scene, ASH::CommandBuffer* cmd, uint32_t frame)
{
    if (!m_ready || !m_skin.ready() || m_skinList.empty() || frame >= kFrames) return;
    if (scene.getVersion() != m_syncedVersion) return;

    VkCommandBuffer vk = reinterpret_cast<VkCommandBuffer>(cmd->getNativeHandle());

    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(vk,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);

    for (auto& r : m_skinList)
    {
      auto* sk = r.obj->getComponent<SkeletalMeshComponent>();
      if (!sk) continue;
      sk->uploadBoneMatrices(frame);
      VkDescriptorSet& s = r.si->sets[frame];
      if (!s) s = m_skin.allocSet(m_ctx, boneVk(*sk, frame));
      r.si->blas.record(m_ctx, m_skin, vk, s);
    }
}

bool World::sync(const ::Scene& scene)
{
    if (!m_ready) return false;
    if (!m_dirty && scene.getVersion() == m_syncedVersion) return true;
    m_dirty = false;
    m_syncedVersion = scene.getVersion();

    std::vector<VkAccelerationStructureInstanceKHR> inst;
    std::vector<GeoEntry> geoe;
    std::vector<DynInst> dyn;
    std::vector<SkinRef> skinList;
    std::unordered_set<const void*> liveSkinned;
    const auto& objs = scene.getObjects();
    for (uint32_t i = 0; i < objs.size(); ++i)
    {
        GameObject& o = *objs[i];
        if (o.isDecal) continue;

        uint64_t asAddr = 0, vtxAddr = 0, idxAddr = 0;
        const std::vector<BlasRange>* rngs = nullptr;
        const std::vector<Submesh>* sms = nullptr;
        SkinInst* skin = nullptr;

        if (o.mesh)
        {
            auto it = m_blas.find(o.mesh.get());
            if (it == m_blas.end()) continue;
            asAddr = it->second->address();
            vtxAddr = it->second->vertexAddress();
            idxAddr = it->second->indexAddress();
            rngs = &it->second->ranges();
            sms = &o.mesh->getSubmeshes();
        }
        else if (auto* sk = o.getComponent<SkeletalMeshComponent>())
        {
            skin = getOrCreateSkinned(o, *sk);
            if (!skin) continue;
            liveSkinned.insert(&o);
            asAddr  = skin->blas.address();
            vtxAddr = skin->blas.vertexAddress();
            idxAddr = skin->blas.indexAddress();
            rngs    = &skin->blas.ranges();
            sms     = &sk->getSkeletalMesh()->getSubmeshes();
        } else continue;

        const glm::mat4 m = o.transform.getMatrix();
        VkAccelerationStructureInstanceKHR in{};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                in.transform.matrix[r][c] = m[c][r];
        const uint32_t base = (uint32_t)geoe.size();
        for (size_t g = 0; g < rngs->size(); ++g)
        {
            GeoEntry e{};
            e.vertexAddr = vtxAddr;
            e.indexAddr = idxAddr;
            e.firstIndex = (*rngs)[g].firstIndex;

            int mi = g < sms->size() ? (int)(*sms)[g].materialIndex : 0;
            uint32_t slot = 0;
            if (!o.materials.empty())
            {
                size_t idx = (mi >= 0 && (size_t)mi < o.materials.size()) ? (size_t)mi : 0;
                if (o.materials[idx]) {
                    auto s = m_matSlots.find(o.materials[idx].get());
                    if (s != m_matSlots.end()) slot = s->second;
                }
            }
            e.materialIndex = slot;
            e.objectIndex   = i;
            geoe.push_back(e);
        }
        in.instanceCustomIndex = base;
        in.mask = 0xFF;
        in.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;
        in.accelerationStructureReference = asAddr;
        if (skin || o.isDynamic || o.hasComponent<RigidBodyComponent>())
            dyn.push_back({ (uint32_t)inst.size(), i });
        if (skin) skinList.push_back({ &o, skin });
        inst.push_back(in);
    }

    uint32_t unmapped = 0;
    for (auto& e : geoe) if (e.materialIndex == 0) ++unmapped;
    LOG(std::string("RT: geometries ") + std::to_string(geoe.size()) +
        ", unmapped materials " + std::to_string(unmapped) + "\n");

    m_ctx.ash()->waitIdle();
    destroyTlas();
    m_instanceCount = (uint32_t)inst.size();
    m_dyn = std::move(dyn);
    m_skinList = std::move(skinList);
    for (auto it = m_skinned.begin(); it != m_skinned.end();)
    {
        if (liveSkinned.count(it->first)) { ++it; continue; }
        it->second->blas.destroy(m_ctx);
        it = m_skinned.erase(it);
    }

    m_geoCount = (uint32_t)geoe.size();
    const size_t geoSize = std::max<size_t>(geoe.size(), 1) * sizeof(GeoEntry);
    m_geoTable = m_ctx.createBuffer(geoSize,
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::CpuToGpu, "RT geometry table");
    if (!geoe.empty()) std::memcpy(m_geoTable->map(), geoe.data(), geoSize);
    else std::memset(m_geoTable->map(), 0, geoSize);

    const size_t instSize = std::max<size_t>(inst.size(), 1) * sizeof(VkAccelerationStructureInstanceKHR);
    for (uint32_t f = 0; f < kFrames; ++f)
    {
        m_instBuf[f] = m_ctx.createBuffer(instSize, ASH::BufferUsage::AccelerationStructureInput,
            ASH::MemoryUsage::CpuToGpu, "RT instances");
        if (!inst.empty()) std::memcpy(m_instBuf[f]->map(), inst.data(), instSize);
        else std::memset(m_instBuf[f]->map(), 0, instSize);
    }

    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geo.flags = 0;
    geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geo.geometry.instances.arrayOfPointers = VK_FALSE;
    geo.geometry.instances.data.deviceAddress = m_instBuf[0]->getDeviceAddress();

    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.geometryCount = 1;
    bi.pGeometries = &geo;

    const uint32_t count = m_instanceCount;
    VkAccelerationStructureBuildSizesInfoKHR sz{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    m_ctx.vkGetASSizes(m_ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, &count, &sz);

    m_tlasStorage = m_ctx.createBuffer(size_t(sz.accelerationStructureSize),
        ASH::BufferUsage::AccelerationStructureStorage, ASH::MemoryUsage::GpuOnly, "RT tlas");
    VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    ci.buffer = reinterpret_cast<VkBuffer>(m_tlasStorage->getNativeHandle());
    ci.size = sz.accelerationStructureSize;
    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    if (m_ctx.vkCreateAS(m_ctx.device(), &ci, nullptr, &m_tlas) != VK_SUCCESS) { destroyTlas(); return false; }

    const uint64_t align = m_ctx.asProps().minAccelerationStructureScratchOffsetAlignment;
    m_tlasScratch = m_ctx.createBuffer(size_t(sz.buildScratchSize + align),
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::GpuOnly, "RT tlas scratch");
    m_scratchAddr = (m_tlasScratch->getDeviceAddress() + align - 1) & ~(align - 1);
    bi.dstAccelerationStructure  = m_tlas;
    bi.scratchData.deviceAddress = m_scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = count;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    if (!m_ctx.submitOneTime([&](VkCommandBuffer cmd) { m_ctx.vkCmdBuildAS(cmd, 1, &bi, &pRange); })) {
        destroyTlas();
        return false;
    }

    m_shadows.setTlas(m_ctx, m_tlas);
    m_reflections.setTlas(m_ctx, m_tlas);
    m_reflections.setGeometry(m_ctx, m_geoTable.get());
    return true;
}

VkImageView World::albedoView(ASH::Texture* t)
{
    constexpr uint32_t kReflMip = 0;
    const uint32_t mips = t->getDesc().mipLevels;
    const uint32_t mip = std::min<uint32_t>(mips ? mips - 1 : 0, kReflMip);
    return reinterpret_cast<VkImageView>(t->getFaceView(0, mip));
}

void World::setDefaultTexture(ASH::Texture* albedo, ASH::Texture* normal)
{
    if (!m_ready || !albedo || !normal || !m_albedoViews.empty()) return;
    m_albedoViews.push_back(albedoView(albedo));
    m_normalViews.push_back(normalView(normal));
    m_ctx.ash()->waitIdle();
    m_reflections.setTextures(m_ctx, m_albedoViews, m_normalViews);
}

uint32_t World::registerMaterial(const void* key, ASH::Texture* albedo, ASH::Texture* normal)
{
    if (!m_ready || m_albedoViews.empty()) return 0;
    if (!albedo) { LOG("RT: material without albedo texture\n"); return 0; }
    if (!m_ready || !albedo || !normal || m_albedoViews.empty()) return 0;
    auto it = m_matSlots.find(key);
    if (it != m_matSlots.end()) return it->second;
    if (m_albedoViews.size() >= ReflectionPass::kMaxTextures) return 0;

    const uint32_t slot = (uint32_t)m_albedoViews.size();
    m_albedoViews.push_back(albedoView(albedo));
    m_normalViews.push_back(normal ? normalView(normal) : m_normalViews[0]);
    m_matSlots[key] = slot;
    m_ctx.ash()->waitIdle();
    m_reflections.setTextures(m_ctx, m_albedoViews, m_normalViews);
    m_dirty = true;
    return slot;
}

}

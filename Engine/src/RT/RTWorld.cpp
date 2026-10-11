#include "RT/RTWorld.h"
#include "Core/Log.h"
#include <cstring>

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
    return m_ready;
}

void World::destroyTlas()
{
    m_shadows.setTlas(m_ctx, VK_NULL_HANDLE);
    if (m_tlas) m_ctx.vkDestroyAS(m_ctx.device(), m_tlas, nullptr);
    m_tlas = VK_NULL_HANDLE;
    m_tlasStorage.reset();
    m_instanceCount = 0;
}

void World::shutdown()
{
    if (!m_ready) return;
    m_ctx.ash()->waitIdle();
    destroyTlas();
    for (auto& kv : m_blas) kv.second->destroy(m_ctx);
    m_blas.clear();
    m_shadows.destroy(m_ctx);
    m_ctx.shutdown();
    m_ready = false;
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

bool World::sync(const ::Scene& scene)
{
    if (!m_ready) return false;
    if (scene.getVersion() == m_syncedVersion) return true;
    m_syncedVersion = scene.getVersion();

    std::vector<VkAccelerationStructureInstanceKHR> inst;
    const auto& objs = scene.getObjects();
    for (uint32_t i = 0; i < objs.size(); ++i) {
        const GameObject& o = *objs[i];
        if (!o.mesh || o.isDecal) continue;
        auto it = m_blas.find(o.mesh.get());
        if (it == m_blas.end()) continue;

        const glm::mat4 m = o.transform.getMatrix();
        VkAccelerationStructureInstanceKHR in{};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                in.transform.matrix[r][c] = m[c][r];
        in.instanceCustomIndex = i;
        in.mask = 0xFF;
        in.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        in.accelerationStructureReference = it->second->address();
        inst.push_back(in);
    }

    m_ctx.ash()->waitIdle();
    destroyTlas();
    m_instanceCount = (uint32_t)inst.size();

    const size_t instSize = std::max<size_t>(inst.size(), 1) * sizeof(VkAccelerationStructureInstanceKHR);
    auto instBuf = m_ctx.createBuffer(instSize, ASH::BufferUsage::AccelerationStructureInput,
        ASH::MemoryUsage::CpuToGpu, "RT instances");
    if (!inst.empty()) std::memcpy(instBuf->map(), inst.data(), instSize);
    else std::memset(instBuf->map(), 0, instSize);

    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geo.geometry.instances.arrayOfPointers = VK_FALSE;
    geo.geometry.instances.data.deviceAddress = instBuf->getDeviceAddress();

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
    auto scratch = m_ctx.createBuffer(size_t(sz.buildScratchSize + align),
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::GpuOnly, "RT tlas scratch");
    bi.dstAccelerationStructure  = m_tlas;
    bi.scratchData.deviceAddress = (scratch->getDeviceAddress() + align - 1) & ~(align - 1);

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = count;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    if (!m_ctx.submitOneTime([&](VkCommandBuffer cmd) { m_ctx.vkCmdBuildAS(cmd, 1, &bi, &pRange); })) {
        destroyTlas();
        return false;
    }

    m_shadows.setTlas(m_ctx, m_tlas);
    return true;
}

}
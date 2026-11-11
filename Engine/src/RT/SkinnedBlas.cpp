#include "RT/SkinnedBlas.h"
#include <algorithm>
#include <cstring>

namespace RT
{

static void memBarrier(VkCommandBuffer cmd, VkPipelineStageFlags src, VkPipelineStageFlags dst)
{
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, src, dst, 0, 1, &b, 0, nullptr, 0, nullptr);
}

bool SkinnedBlas::create(Context& ctx, const void* srcVertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount,
    const BlasRange* ranges, uint32_t rangeCount)
{
    if (!srcVertices || !indices || vertexCount == 0 || indexCount < 3) return false;

    constexpr uint32_t kSrcStride = 19 * sizeof(float);
    constexpr uint32_t kDstStride = 64;

    const size_t srcSize = size_t(vertexCount) * kSrcStride;
    const size_t dstSize = size_t(vertexCount) * kDstStride;
    const size_t ibSize = size_t(indexCount) * sizeof(uint32_t);

    m_src = ctx.createBuffer(srcSize,
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::CpuToGpu, "RT skin src");
    m_dst = ctx.createBuffer(dstSize,
        ASH::BufferUsage::AccelerationStructureInput | ASH::BufferUsage::StorageBuffer |
        ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::GpuOnly, "RT skin dst");
    m_ib = ctx.createBuffer(ibSize,
        ASH::BufferUsage::AccelerationStructureInput | ASH::BufferUsage::StorageBuffer |
        ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::CpuToGpu, "RT skin ib");
    if (!m_src || !m_dst || !m_ib) { destroy(ctx); return false; }

    std::memcpy(m_src->map(), srcVertices, srcSize);
    std::memcpy(m_ib->map(), indices, ibSize);

    const BlasRange whole{0, indexCount};
    const BlasRange* rg = ranges ? ranges : &whole;
    const uint32_t rc = ranges ? rangeCount : 1;
    m_ranges.assign(rg, rg + rc);
    m_vertexCount = vertexCount;

    m_geos.assign(rc, {});
    m_infos.assign(rc, {});
    std::vector<uint32_t> prims(rc);
    for (uint32_t i = 0; i < rc; ++i)
    {
        m_geos[i] = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        m_geos[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        m_geos[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        auto& t = m_geos[i].geometry.triangles;
        t.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        t.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        t.vertexData.deviceAddress = m_dst->getDeviceAddress();
        t.vertexStride = kDstStride;
        t.maxVertex = vertexCount - 1;
        t.indexType = VK_INDEX_TYPE_UINT32;
        t.indexData.deviceAddress = m_ib->getDeviceAddress();

        m_infos[i].primitiveCount = rg[i].indexCount / 3;
        m_infos[i].primitiveOffset = rg[i].firstIndex * sizeof(uint32_t);
        prims[i] = m_infos[i].primitiveCount;
    }

    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    bi.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.geometryCount = rc;
    bi.pGeometries = m_geos.data();

    VkAccelerationStructureBuildSizesInfoKHR sz{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    ctx.vkGetASSizes(ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, prims.data(), &sz);

    m_storage = ctx.createBuffer(size_t(sz.accelerationStructureSize),
        ASH::BufferUsage::AccelerationStructureStorage, ASH::MemoryUsage::GpuOnly, "RT skin blas");
    VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    ci.buffer = reinterpret_cast<VkBuffer>(m_storage->getNativeHandle());
    ci.size = sz.accelerationStructureSize;
    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if (ctx.vkCreateAS(ctx.device(), &ci, nullptr, &m_as) != VK_SUCCESS) { destroy(ctx); return false; }

    const uint64_t align = ctx.asProps().minAccelerationStructureScratchOffsetAlignment;
    const uint64_t scratchSize = std::max(sz.buildScratchSize, sz.updateScratchSize);
    m_scratch = ctx.createBuffer(size_t(scratchSize + align),
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::GpuOnly, "RT skin scratch");
    m_scratchAddr = (m_scratch->getDeviceAddress() + align - 1) & ~(align - 1);

    VkAccelerationStructureDeviceAddressInfoKHR ai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    ai.accelerationStructure = m_as;
    m_address = ctx.vkGetASAddr(ctx.device(), &ai);
    m_built = false;
    return true;
}

void SkinnedBlas::record(Context& ctx, SkinPass& skin, VkCommandBuffer cmd, VkDescriptorSet boneSet)
{
    if (!valid() || !boneSet) return;

    skin.dispatch(cmd, boneSet, m_src->getDeviceAddress(), m_dst->getDeviceAddress(), m_vertexCount);
    memBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    bi.mode  = m_built ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
        : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.srcAccelerationStructure = m_built ? m_as : VK_NULL_HANDLE;
    bi.dstAccelerationStructure = m_as;
    bi.geometryCount = uint32_t(m_geos.size());
    bi.pGeometries = m_geos.data();
    bi.scratchData.deviceAddress = m_scratchAddr;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = m_infos.data();
    ctx.vkCmdBuildAS(cmd, 1, &bi, &pRange);
    m_built = true;

    memBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
}

void SkinnedBlas::destroy(Context& ctx)
{
    if (m_as) ctx.vkDestroyAS(ctx.device(), m_as, nullptr);
    m_as = VK_NULL_HANDLE;
    m_address = 0;
    m_built = false;
    m_scratch.reset(); m_storage.reset();
    m_ib.reset(); m_dst.reset(); m_src.reset();
    m_ranges.clear(); m_geos.clear(); m_infos.clear();
}

}

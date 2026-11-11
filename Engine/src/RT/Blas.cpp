#include "RT/Blas.h"
#include <cstring>

namespace RT {

static std::unique_ptr<ASH::Buffer> makeBuffer(Context& ctx, size_t size,
    ASH::BufferUsage usage, ASH::MemoryUsage mem, const char* name)
{
    ASH::BufferDesc d;
    d.size = size; d.usage = usage; d.memory = mem; d.debugName = name;
    return ctx.ash()->createBuffer(d);
}

bool Blas::build(Context& ctx, const BlasInput& in)
{
    if (!in.vertices || !in.indices || in.vertexCount == 0 || in.indexCount < 3) return false;

    const size_t vbSize = size_t(in.vertexCount) * in.vertexStride;
    const size_t ibSize = size_t(in.indexCount) * sizeof(uint32_t);
    auto vb = makeBuffer(ctx, vbSize, ASH::BufferUsage::AccelerationStructureInput, ASH::MemoryUsage::CpuToGpu, "RT vb");
    auto ib = makeBuffer(ctx, ibSize, ASH::BufferUsage::AccelerationStructureInput, ASH::MemoryUsage::CpuToGpu, "RT ib");
    std::memcpy(vb->map(), in.vertices, vbSize);
    std::memcpy(ib->map(), in.indices,  ibSize);

    const BlasRange whole{0, in.indexCount};
    const BlasRange* ranges = in.ranges ? in.ranges : &whole;
    const uint32_t rangeCount = in.ranges ? in.rangeCount : 1;

    std::vector<VkAccelerationStructureGeometryKHR>       geos(rangeCount);
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> infos(rangeCount);
    std::vector<uint32_t>                                 prims(rangeCount);

    for (uint32_t i = 0; i < rangeCount; ++i) {
        geos[i] = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geos[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geos[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        auto& t = geos[i].geometry.triangles;
        t.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        t.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        t.vertexData.deviceAddress = vb->getDeviceAddress();
        t.vertexStride = in.vertexStride;
        t.maxVertex = in.vertexCount - 1;
        t.indexType = VK_INDEX_TYPE_UINT32;
        t.indexData.deviceAddress = ib->getDeviceAddress();

        infos[i] = {};
        infos[i].primitiveCount  = ranges[i].indexCount / 3;
        infos[i].primitiveOffset = ranges[i].firstIndex * sizeof(uint32_t);
        prims[i] = infos[i].primitiveCount;
    }

    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type  = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    bi.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.geometryCount = rangeCount;
    bi.pGeometries = geos.data();

    VkAccelerationStructureBuildSizesInfoKHR sz{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    ctx.vkGetASSizes(ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, prims.data(), &sz);

    m_storage = makeBuffer(ctx, size_t(sz.accelerationStructureSize),
        ASH::BufferUsage::AccelerationStructureStorage, ASH::MemoryUsage::GpuOnly, "RT blas");
    VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    ci.buffer = reinterpret_cast<VkBuffer>(m_storage->getNativeHandle());
    ci.size   = sz.accelerationStructureSize;
    ci.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if (ctx.vkCreateAS(ctx.device(), &ci, nullptr, &m_as) != VK_SUCCESS) { m_storage.reset(); return false; }

    const uint64_t align = ctx.asProps().minAccelerationStructureScratchOffsetAlignment;
    auto scratch = makeBuffer(ctx, size_t(sz.buildScratchSize + align),
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::GpuOnly, "RT scratch");
    uint64_t sAddr = (scratch->getDeviceAddress() + align - 1) & ~(align - 1);

    bi.dstAccelerationStructure = m_as;
    bi.scratchData.deviceAddress = sAddr;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = infos.data();

    if (!ctx.submitOneTime([&](VkCommandBuffer cmd) { ctx.vkCmdBuildAS(cmd, 1, &bi, &pRange); })) {
        destroy(ctx);
        return false;
    }

    VkAccelerationStructureDeviceAddressInfoKHR ai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    ai.accelerationStructure = m_as;
    m_vb = std::move(vb);
    m_ib = std::move(ib);
    m_ranges.assign(ranges, ranges + rangeCount);
    m_address = ctx.vkGetASAddr(ctx.device(), &ai);
    return true;
}

void Blas::destroy(Context& ctx)
{
    if (m_as) ctx.vkDestroyAS(ctx.device(), m_as, nullptr);
    m_as = VK_NULL_HANDLE;
    m_address = 0;
    m_vb.reset();
    m_ib.reset();
    m_ranges.clear();
    m_storage.reset();
}

bool Blas::create(Context& ctx, const void* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount,
    const BlasRange* ranges, uint32_t rangeCount)
{
    BlasInput in;
    in.vertices = vertices;
    in.vertexCount = vertexCount;
    in.vertexStride = sizeof(float) * 16;
    in.indices = indices;
    in.indexCount = indexCount;
    in.ranges = ranges;
    in.rangeCount = rangeCount;

    if (!build(ctx, in)) return false;

    const auto& props = ctx.asProps();
    const uint64_t align = props.minAccelerationStructureScratchOffsetAlignment;

    VkAccelerationStructureBuildSizesInfoKHR sz{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};

    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geo.flags = 0;
    geo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geo.geometry.triangles.maxVertex = vertexCount;
    geo.geometry.triangles.vertexStride = in.vertexStride;
    geo.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geo.geometry.triangles.vertexData.deviceAddress = m_vb->getDeviceAddress();
    geo.geometry.triangles.indexData.deviceAddress = m_ib->getDeviceAddress();

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;
    for (uint32_t i = 0; i < rangeCount; ++i) {
        VkAccelerationStructureBuildRangeInfoKHR r{};
        r.primitiveCount = ranges[i].indexCount / 3;
        r.firstVertex = 0;
        r.primitiveOffset = ranges[i].firstIndex * sizeof(uint32_t);
        r.transformOffset = 0;
        rangeInfos.push_back(r);
    }

    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.geometryCount = 1;
    bi.pGeometries = &geo;
    bi.dstAccelerationStructure = m_as;

    const VkAccelerationStructureBuildRangeInfoKHR* pRanges = rangeInfos.data();
    ctx.vkGetASSizes(ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, &pRanges[0].primitiveCount, &sz);

    m_updateScratch = ctx.createBuffer(size_t(sz.updateScratchSize + align),
        ASH::BufferUsage::StorageBuffer | ASH::BufferUsage::ShaderDeviceAddress,
        ASH::MemoryUsage::GpuOnly, "RT blas update scratch");
    m_updateScratchAddr = (m_updateScratch->getDeviceAddress() + align - 1) & ~(align - 1);

    return true;
}

void Blas::record(Context& ctx, const SkinPass& skin, VkCommandBuffer cmd, VkDescriptorSet set)
{
    if (!m_as || !m_updateScratch) return;


}

}

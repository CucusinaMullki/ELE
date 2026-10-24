#pragma once
#include "RT/Blas.h"
#include "RT/RTSkinPass.h"
#include <vector>

namespace RT
{
class SkinnedBlas
{
public:
    bool create(Context& ctx, const void* srcVertices, uint32_t vertexCount,
        const uint32_t* indices, uint32_t indexCount,
        const BlasRange* ranges, uint32_t rangeCount);
    void destroy(Context& ctx);

    void record(Context& ctx, SkinPass& skin, VkCommandBuffer cmd, VkDescriptorSet boneSet);

    bool valid() const { return m_as != VK_NULL_HANDLE; }
    uint64_t address() const { return m_address; }
    uint64_t vertexAddress() const { return m_dst ? m_dst->getDeviceAddress() : 0; }
    uint64_t indexAddress()  const { return m_ib ? m_ib->getDeviceAddress() : 0; }
    const std::vector<BlasRange>& ranges() const { return m_ranges; }

private:
    std::unique_ptr<ASH::Buffer> m_src, m_dst, m_ib, m_storage, m_scratch;
    VkAccelerationStructureKHR m_as = VK_NULL_HANDLE;
    uint64_t m_address = 0;
    uint32_t m_vertexCount = 0;
    uint64_t m_scratchAddr = 0;
    bool m_built = false;

    std::vector<BlasRange> m_ranges;
    std::vector<VkAccelerationStructureGeometryKHR> m_geos;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> m_infos;
};
}
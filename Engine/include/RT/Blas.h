#pragma once
#include "RT/RTContext.h"
#include "RT/RTSkinPass.h"
#include <ASH/Buffer.h>
#include <memory>
#include <vector>

namespace RT
{

struct BlasRange
{
    uint32_t firstIndex;
    uint32_t indexCount;
};

struct BlasInput
{
    const void* vertices = nullptr;
    uint32_t vertexCount = 0;
    uint32_t vertexStride = 0;
    const uint32_t* indices = nullptr;
    uint32_t indexCount = 0;
    const BlasRange* ranges = nullptr;
    uint32_t rangeCount = 0;
};

class Blas
{
public:
    bool build(Context& ctx, const BlasInput& in);

    bool create(Context& ctx, const void* vertices, uint32_t vertexCount,
        const uint32_t* indices, uint32_t indexCount,
        const BlasRange* ranges, uint32_t rangeCount);

    void record(Context& ctx, const SkinPass& skin, VkCommandBuffer cmd, VkDescriptorSet set);

    void destroy(Context& ctx);

    uint64_t vertexAddress() const { return m_vb ? m_vb->getDeviceAddress() : 0; }
    uint64_t indexAddress()  const { return m_ib ? m_ib->getDeviceAddress() : 0; }
    const std::vector<BlasRange>& ranges() const { return m_ranges; }

    bool valid() const { return m_as != VK_NULL_HANDLE; }
    uint64_t address() const { return m_address; }

private:
    std::unique_ptr<ASH::Buffer> m_storage;
    std::unique_ptr<ASH::Buffer> m_updateScratch;
    uint64_t m_updateScratchAddr = 0;

    VkAccelerationStructureKHR m_as = VK_NULL_HANDLE;
    uint64_t m_address = 0;
    std::unique_ptr<ASH::Buffer> m_vb, m_ib;
    std::vector<BlasRange> m_ranges;
};

}

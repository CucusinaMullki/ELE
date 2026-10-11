#pragma once
#include "RT/RTContext.h"
#include <ASH/Buffer.h>
#include <memory>

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
    void destroy(Context& ctx);

    bool valid() const { return m_as != VK_NULL_HANDLE; }
    uint64_t address() const { return m_address; }

private:
    std::unique_ptr<ASH::Buffer> m_storage;
    VkAccelerationStructureKHR m_as = VK_NULL_HANDLE;
    uint64_t m_address = 0;
};

}
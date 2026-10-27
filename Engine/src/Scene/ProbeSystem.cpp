#include "Scene/ProbeSystem.h"
#include <cstring>

void ProbeSystem::initialize(ASH::Device* device)
{
    ASH::BufferDesc dd{};
    dd.size = size_t(kMaxProbes) * ProbeGrid::kFloatsPerProbe * sizeof(float);
    dd.usage = ASH::BufferUsage::StorageBuffer;
    dd.memory = ASH::MemoryUsage::CpuToGpu;
    m_dataBuffer = device->createBuffer(dd);
    void* p = m_dataBuffer->map();
    std::memset(p, 0, dd.size);
    m_dataBuffer->unmap();

    ASH::BufferDesc id{};
    id.size = sizeof(glm::vec4) + sizeof(glm::uvec4);
    id.usage = ASH::BufferUsage::UniformBuffer;
    id.memory = ASH::MemoryUsage::CpuToGpu;
    m_infoBuffer = device->createBuffer(id);

    writeInfo(glm::vec3(0.0f), 1.0f, glm::uvec3(0u), false);
}

bool ProbeSystem::load(const std::string& path)
{
    ProbeGrid grid;
    if (path.empty() || !grid.load(path) || grid.probeCount() > kMaxProbes)
    {
        if (!path.empty())
            std::fprintf(stderr, "[Probes] disabled (load failed or > %u probes)\n", kMaxProbes);
        m_enabled = false;
        m_probeCount = 0;
        writeInfo(glm::vec3(0.0f), 1.0f, glm::uvec3(0u), false);
        return false;
    }

    void* p = m_dataBuffer->map();
    std::memcpy(p, grid.data.data(), grid.byteSize());
    m_dataBuffer->unmap();

    m_enabled = true;
    writeInfo(grid.origin, grid.spacing, grid.dims, true);

    m_probeCount = grid.probeCount();

    return true;
}

void ProbeSystem::writeInfo(const glm::vec3& origin, float spacing, const glm::uvec3& dims, bool enabled)
{
    struct Info { glm::vec4 originSpacing; glm::uvec4 dims; } info;
    info.originSpacing = glm::vec4(origin, spacing);
    info.dims          = glm::uvec4(dims, enabled ? 1u : 0u);
    void* p = m_infoBuffer->map();
    std::memcpy(p, &info, sizeof(info));
    m_infoBuffer->unmap();
}
#pragma once
#include "Scene/ProbeGrid.h"
#include <ASH/ASH.h>
#include <memory>
#include <string>

class ProbeSystem
{
public:
    static constexpr uint32_t kMaxProbes = 32768;

    void initialize(ASH::Device* device);
    void shutdown() { m_dataBuffer.reset(); m_infoBuffer.reset(); m_enabled = false; }
    bool load(const std::string& eprobesPath);

    ASH::Buffer* getDataBuffer() const { return m_dataBuffer.get(); }
    uint32_t getProbeCount() const { return m_probeCount; }
    ASH::Buffer* getInfoBuffer() const { return m_infoBuffer.get(); }
    bool isEnabled() const { return m_enabled; }

private:
    void writeInfo(const glm::vec3& origin, float spacing, const glm::uvec3& dims, bool enabled);

    std::unique_ptr<ASH::Buffer> m_dataBuffer;
    std::unique_ptr<ASH::Buffer> m_infoBuffer;
    bool m_enabled = false;
    uint32_t m_probeCount = 0;
};

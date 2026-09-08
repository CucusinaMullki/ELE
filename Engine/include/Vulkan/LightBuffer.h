#pragma once
#include "Scene/Light.h"
#include "ASH/ASH.h"
#include <vector>
#include <memory>

static const int MAX_LIGHTS = 16;

struct LightData
{
    Light lights[MAX_LIGHTS];
    glm::ivec4 count;
};

class LightBuffer
{
public:
    void initialize(ASH::Device* device, uint32_t frameCount);
    void update(uint32_t frameIndex, const LightData& data);
    void cleanup();

    ASH::Buffer* getBuffer(uint32_t frameIndex) const { return m_buffers[frameIndex].get(); }

private:
    std::vector<std::unique_ptr<ASH::Buffer>> m_buffers;
};
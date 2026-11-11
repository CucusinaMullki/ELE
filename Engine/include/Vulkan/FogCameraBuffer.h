#pragma once
#include "ASH/ASH.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

struct FogCameraData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invViewProj;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 camPos;
};

class FogCameraBuffer
{
public:
    void initialize(ASH::Device* device, uint32_t frameCount);
    void update(uint32_t frameIndex, const FogCameraData& data);
    void cleanup();
    ASH::Buffer* getBuffer(uint32_t frameIndex) const { return m_buffers[frameIndex].get(); }

private:
    std::vector<std::unique_ptr<ASH::Buffer>> m_buffers;
};

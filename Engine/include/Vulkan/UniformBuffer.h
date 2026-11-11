#pragma once
#include "ASH/ASH.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

struct MVP
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightSpaceMatrix;
    glm::mat4 prevViewProj;
    glm::mat4 viewProjNoJitter;
    glm::vec4 camPos;
    glm::vec4 iblParams;
};

class UniformBuffer
{
public:
    void initialize(ASH::Device* device, uint32_t frameCount);
    void update(uint32_t frameIndex, const MVP& mvp);
    void cleanup();

    ASH::Buffer* getBuffer(uint32_t frameIndex) const { return m_buffers[frameIndex].get(); }

private:
    std::vector<std::unique_ptr<ASH::Buffer>> m_buffers;
};

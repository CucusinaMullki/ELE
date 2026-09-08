#pragma once
#include "ASH/ASH.h"
#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <memory>

static constexpr int MAX_BONES = 100;

struct BoneMatrixData
{
    std::array<glm::mat4, MAX_BONES> bones;
};

class BoneMatrixBuffer
{
public:
    void initialize(ASH::Device* device, uint32_t frameCount);
    void update(uint32_t frameIndex, const BoneMatrixData& data);
    void cleanup();
    ASH::Buffer* getBuffer(uint32_t frameIndex) const { return m_buffers[frameIndex].get(); }

private:
    std::vector<std::unique_ptr<ASH::Buffer>> m_buffers;
};
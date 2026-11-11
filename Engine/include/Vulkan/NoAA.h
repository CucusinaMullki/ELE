#pragma once
#include "Vulkan/AntiAliasing.h"

class NoAA : public AntiAliasing
{
public:
    void initialize(ASH::Device*, ASH::Extent2D) override {}
    void cleanup() override {}
    void apply(ASH::CommandBuffer* cmd, uint32_t frameIndex, ASH::Texture* srcTexture, ASH::Sampler* srcSampler) override {}
    AAMethod getMethod() const override { return AAMethod::None; }
};

#pragma once
#include "ASH/ASH.h"

enum class AAMethod
{
    None,
    FXAA,
    TAA
};

class AntiAliasing
{
public:
    virtual ~AntiAliasing() = default;
    virtual void initialize(ASH::Device* device, ASH::Extent2D extent) = 0;
    virtual void cleanup() = 0;
    virtual void apply(ASH::CommandBuffer* cmd, uint32_t frameIndex, ASH::Texture* srcTexture, ASH::Sampler* srcSampler) = 0;
    virtual AAMethod getMethod() const = 0;
};

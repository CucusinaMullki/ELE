#pragma once
#include "Vulkan/AntiAliasing.h"

class NoAA : public AntiAliasing
{
public:
    void initialize(vk::Device, vk::PhysicalDevice, vk::Extent2D, vk::PipelineCache) override {}
    void cleanup() override {}
    void apply(vk::CommandBuffer, uint32_t, vk::ImageView, vk::Sampler) override {}
    AAMethod getMethod() const override { return AAMethod::None; }
};
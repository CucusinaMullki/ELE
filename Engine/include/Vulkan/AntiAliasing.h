#pragma once
#include <vulkan/vulkan.hpp>

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

    virtual void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent, vk::PipelineCache cache) = 0;
    virtual void cleanup() = 0;

    virtual void apply(vk::CommandBuffer cmd, uint32_t frameIndex, vk::ImageView srcView, vk::Sampler srcSampler) = 0;

    virtual AAMethod getMethod() const = 0;
    
};
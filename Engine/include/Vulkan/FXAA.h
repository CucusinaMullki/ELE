#pragma once
#include "Vulkan/AntiAliasing.h"
#include "Vulkan/Pipeline.h"
#include <vector>

class FXAA : public AntiAliasing
{
public:
    void initializeWithRenderPass(vk::Device device, vk::PipelineCache cache,
        vk::RenderPass renderPass, uint32_t frameCount);

    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice,
        vk::Extent2D extent, vk::PipelineCache cache) override {}
    void cleanup() override;
    void apply(vk::CommandBuffer cmd, uint32_t frameIndex,
        vk::ImageView srcView, vk::Sampler srcSampler) override;
    AAMethod getMethod() const override { return AAMethod::FXAA; }

    ASH::Pipeline* getPipeline() const { return m_pipeline.getPipeline(); }

    void updateDescriptorSet(vk::Device device, uint32_t frameIndex, vk::ImageView srcView, vk::Sampler srcSampler);

private:
    Pipeline m_pipeline;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};
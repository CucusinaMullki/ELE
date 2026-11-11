#pragma once
#include "Vulkan/AntiAliasing.h"
#include "Vulkan/Pipeline.h"
#include <vector>
#include <memory>

class FXAA : public AntiAliasing
{
public:
    void initializeWithRenderPass(ASH::Device* device, ASH::RenderPass* renderPass, uint32_t frameCount);
    void initialize(ASH::Device* device, ASH::Extent2D extent) override {}
    void cleanup() override;
    void apply(ASH::CommandBuffer* cmd, uint32_t frameIndex, ASH::Texture* srcTexture, ASH::Sampler* srcSampler) override;
    AAMethod getMethod() const override { return AAMethod::FXAA; }
    ASH::Pipeline* getPipeline() const { return m_pipeline.getPipeline(); }
    void updateDescriptorSet(uint32_t frameIndex, ASH::Texture* srcTexture, ASH::Sampler* srcSampler);

private:
    Pipeline m_pipeline;
    std::vector<std::unique_ptr<ASH::DescriptorSet>> m_descriptorSets;
};

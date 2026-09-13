#pragma once
#include "ASH/ASH.h"
#include "Vulkan/Pipeline.h"
#include <string>
#include <memory>

struct  ComputePipelineConfig
{
    std::string computeShader;
    LayoutType layoutType = LayoutType::None;
    uint32_t pushConstantSize = 0;
    ASH::ShaderStage pushStage = ASH::ShaderStage::Compute;
};

class ComputePipeline
{
public:
    void initialize(ASH::Device* device, const ComputePipelineConfig& config);
    void cleanup();

    ASH::DescriptorSetLayout* getDescriptorSetLayout() const { return m_descriptorSetLayout.get(); }
    std::unique_ptr<ASH::DescriptorSet> createDescriptorSet() { return m_device->createDescriptorSet(m_descriptorSetLayout.get()); }
    ASH::Pipeline* getPipeline() const { return m_pipeline.get(); }

private:
    ASH::Device* m_device = nullptr;
    std::unique_ptr<ASH::DescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<ASH::Pipeline> m_pipeline;
};
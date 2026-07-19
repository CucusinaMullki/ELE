#pragma once
#include "vulkan/vulkan.hpp"
#include <string>
#include <vector>

class Pipeline
{
public:
    void initialize(vk::Device device, vk::RenderPass renderPass, const std::string& shaderName, uint32_t frameCount, vk::PipelineCache cache = nullptr);
    void cleanup();

    vk::DescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout.get(); }
    vk::DescriptorPool getDescriptorPool() const { return m_descriptorPool.get(); }
    vk::PipelineLayout getLayout() const { return m_pipelineLayout.get(); }
    vk::Pipeline getPipeline() const { return m_graphicPipeline.get(); }

private:
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicPipeline;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    vk::UniqueDescriptorPool m_descriptorPool;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};
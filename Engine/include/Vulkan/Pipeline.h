#pragma once
#include "vulkan/vulkan.hpp"
#include <string>
#include <vector>

class Pipeline
{
public:
    void initialize(vk::Device device, vk::RenderPass renderPass, const std::string& shaderName, uint32_t frameCount);
    void cleanup();
    void updateDescriptorSets(vk::Device device, const std::vector<vk::DescriptorBufferInfo>& bufferInfos, vk::ImageView textureImageView, vk::Sampler textureSampler);

    vk::Pipeline getPipeline() const { return m_graphicPipeline.get(); } 
    vk::PipelineLayout getLayout() const { return m_pipelineLayout.get(); }
    vk::DescriptorSet getDescriptorSet(uint32_t frameIndex) const { return m_descriptorSets[frameIndex]; }


private:
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicPipeline;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    vk::UniqueDescriptorPool m_descriptorPool;
    std::vector<vk::DescriptorSet> m_descriptorSets;
};
#pragma once
#include "vulkan/vulkan.hpp"
#include <string>
#include <vector>

struct PipelineConfig
{
    std::string vertexShader;
    std::string fragmentShader = "";
    vk::RenderPass renderPass;
    vk::PipelineCache cache = nullptr;
    uint32_t frameCount = 1;
    uint32_t maxMaterials = 100;

    bool useDescriptors = true;
    vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eBack;
    bool depthClampEnable = false;
    uint32_t colorAttachmentCount = 1;
    bool depthWrite = true;
    bool depthTest = true;
    bool blendEnable = false;

    bool depthBiasEnable = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
};

class Pipeline
{
public:
    void initialize(vk::Device device, const PipelineConfig& config);
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
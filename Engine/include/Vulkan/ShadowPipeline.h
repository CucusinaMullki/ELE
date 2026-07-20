#pragma once
#include <vulkan/vulkan.hpp>
#include "Vulkan/Shader.h"

class ShadowPipeline
{
public:
    void initialize(vk::Device device, vk::RenderPass renderPass, vk::PipelineCache cache);
    void cleanup();

    vk::Pipeline getPipeline() const { return m_pipeline.get(); }
    vk::PipelineLayout getLayout() const { return m_layout.get(); }
private:
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_layout;
    Shader m_vertexShader;
};
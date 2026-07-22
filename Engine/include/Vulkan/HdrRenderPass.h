#pragma once
#include <vulkan/vulkan.hpp>

class HdrRenderPass
{
public:
    void initialize(vk::Device device, vk::Format colorFormat, vk::Format depthFormat);
    void cleanup();

    vk::RenderPass getRenderPass() const { return m_renderPass.get(); }

private:
    vk::UniqueRenderPass m_renderPass;
};
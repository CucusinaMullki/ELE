#pragma once
#include <vulkan/vulkan.hpp>

class ShadowRenderPass
{
public:
    void initialize(vk::Device device, vk::Format depthFormat);
    void cleanup();

    vk::RenderPass getRenderPass() const { return m_renderPass.get(); }
    
private:
    vk::UniqueRenderPass m_renderPass;
};
#pragma once
#include <vulkan/vulkan.hpp>

class PostRenderPass
{
public:
    void initialize(vk::Device device, vk::Format colorFormat);
    void cleanup();
    vk::RenderPass getRenderPass() const { return m_renderPass.get(); }

private:
    vk::UniqueRenderPass m_renderPass;
};
#pragma once

#include <vulkan/vulkan.hpp>

class RenderPass
{
public:
    RenderPass() = default;
    ~RenderPass() { cleanup(); }

    void initialize(vk::Device device, vk::Format swapchainFormat, vk::Format depthFormat);
    void cleanup();

    vk::RenderPass getRenderPass() const { return m_renderPass.get(); }

private:
    vk::UniqueRenderPass m_renderPass;
};
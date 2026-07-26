#pragma once
#include <vulkan/vulkan.hpp>

class PostRenderPass
{
public:
    void initialize(vk::Device device, vk::Format colorFormat, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear);
    void cleanup();
    vk::RenderPass getRenderPass() const { return m_renderPass.get(); }

private:
    vk::UniqueRenderPass m_renderPass;
};
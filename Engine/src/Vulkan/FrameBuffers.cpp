#include "Vulkan/FrameBuffers.h"

void Framebuffers::initialize(vk::Device device, const SwapChain& swapChain, vk::RenderPass renderPass, vk::ImageView depthImageView)
{
    const auto& imageViews = swapChain.getImageViews();
    vk::Extent2D extent = swapChain.getExtent();

    m_framebuffers.resize(swapChain.getImageViews().size());

    for (size_t i = 0; i < imageViews.size(); i++)
    {
        std::array<vk::ImageView, 2> attachments = {
            imageViews[i].get(),
            depthImageView
        };

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.setRenderPass(renderPass);
        framebufferInfo.setAttachments(attachments);
        framebufferInfo.setWidth(extent.width);
        framebufferInfo.setHeight(extent.height);
        framebufferInfo.setLayers(1);

        m_framebuffers[i] = device.createFramebufferUnique(framebufferInfo);
    }
}

void Framebuffers::cleanup()
{
    m_framebuffers.clear();
}
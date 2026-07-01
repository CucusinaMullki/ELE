#include "Vulkan/FrameBuffers.h"

void Framebuffers::initialize(vk::Device device, const SwapChain& swapChain, vk::RenderPass renderPass)
{
    m_framebuffers.resize(swapChain.getImageViews().size());

    for (size_t i = 0; i < swapChain.getImageViews().size(); i++)
    {
        vk::ImageView attachments[] = {
           swapChain.getImageViews()[i].get()
        };

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.setRenderPass(renderPass);
        framebufferInfo.setAttachments(attachments);
        framebufferInfo.setWidth(swapChain.getExtent().width);
        framebufferInfo.setHeight(swapChain.getExtent().height);
        framebufferInfo.setLayers(1);

        m_framebuffers[i] = device.createFramebufferUnique(framebufferInfo);
    }
}

void Framebuffers::cleanup()
{
    m_framebuffers.clear();
}
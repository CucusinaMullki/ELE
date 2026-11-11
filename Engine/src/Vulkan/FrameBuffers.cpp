#include "Vulkan/FrameBuffers.h"

void Framebuffers::initialize(ASH::Device* device, const SwapChain& swapChain, ASH::RenderPass* renderPass, ASH::Texture* depthTexture)
{
    uint32_t imageCount = swapChain.getImageCount();
    ASH::Extent2D extent = swapChain.getExtent();

    m_framebuffers.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++)
    {
        ASH::FramebufferDesc desc{};
        desc.renderPass = renderPass;
        desc.colorAttachments.push_back(swapChain.getImage(i));
        desc.depthStencilAttachment = depthTexture;
        desc.extent = extent;
        desc.layers = 1;

        m_framebuffers[i] = device->createFramebuffer(desc);
    }
}

void Framebuffers::cleanup()
{
    m_framebuffers.clear();
}

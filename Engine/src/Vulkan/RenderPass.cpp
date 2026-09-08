#include "Vulkan/RenderPass.h"

void RenderPass::initialize(ASH::Device* device, ASH::Format swapchainFormat, ASH::Format depthFormat)
{
    ASH::AttachmentDesc colorAttachment{};
    colorAttachment.format = swapchainFormat;
    colorAttachment.sampleCount = 1;
    colorAttachment.loadOp = ASH::LoadOp::Clear;
    colorAttachment.storeOp = ASH::StoreOp::Store;
    colorAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    colorAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    ASH::RenderPassDesc desc{};
    desc.colorAttachments.push_back(colorAttachment);
    desc.hasDepthStencil = true;
    desc.depthStencilAttachment.format = depthFormat;
    desc.depthStencilAttachment.sampleCount = 1;
    desc.depthStencilAttachment.loadOp = ASH::LoadOp::Clear;
    desc.depthStencilAttachment.storeOp = ASH::StoreOp::DontCare;
    desc.depthStencilAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    desc.depthStencilAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    m_renderPass = device->createRenderPass(desc);
}

void RenderPass::cleanup()
{
    m_renderPass.reset();
}
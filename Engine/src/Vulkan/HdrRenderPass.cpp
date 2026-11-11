#include "Vulkan/HdrRenderPass.h"

void HdrRenderPass::initialize(ASH::Device* device, ASH::Format colorFormat, ASH::Format velocityFormat, ASH::Format depthFormat)
{
    ASH::AttachmentDesc colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.sampleCount = 1;
    colorAttachment.loadOp = ASH::LoadOp::Clear;
    colorAttachment.storeOp = ASH::StoreOp::Store;
    colorAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    colorAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    ASH::AttachmentDesc velocityAttachment{};
    velocityAttachment.format = velocityFormat;
    velocityAttachment.sampleCount = 1;
    velocityAttachment.loadOp = ASH::LoadOp::Clear;
    velocityAttachment.storeOp = ASH::StoreOp::Store;
    velocityAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    velocityAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    ASH::RenderPassDesc desc{};
    desc.colorAttachments.push_back(colorAttachment);
    desc.colorAttachments.push_back(velocityAttachment);
    desc.hasDepthStencil = true;
    desc.depthStencilAttachment.format = depthFormat;
    desc.depthStencilAttachment.sampleCount = 1;
    desc.depthStencilAttachment.loadOp = ASH::LoadOp::Clear;
    desc.depthStencilAttachment.storeOp = ASH::StoreOp::Store;
    desc.depthStencilAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    desc.depthStencilAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    m_renderPass = device->createRenderPass(desc);
}

void HdrRenderPass::cleanup()
{
    m_renderPass.reset();
}

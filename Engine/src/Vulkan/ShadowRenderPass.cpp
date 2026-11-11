#include "Vulkan/ShadowRenderPass.h"

void ShadowRenderPass::initialize(ASH::Device* device, ASH::Format depthFormat)
{
    ASH::RenderPassDesc desc{};
    desc.hasDepthStencil = true;
    desc.depthStencilAttachment.format = depthFormat;
    desc.depthStencilAttachment.sampleCount = 1;
    desc.depthStencilAttachment.loadOp = ASH::LoadOp::Clear;
    desc.depthStencilAttachment.storeOp = ASH::StoreOp::Store;
    desc.depthStencilAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    desc.depthStencilAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    m_renderPass = device->createRenderPass(desc);
}

void ShadowRenderPass::cleanup()
{
    m_renderPass.reset();
}

#include "Vulkan/PointShadowRenderPass.h"

void PointShadowRenderPass::initialize(ASH::Device* device, ASH::Format depthFormat, bool loadExisting)
{
    ASH::RenderPassDesc desc{};
    desc.hasDepthStencil = true;
    desc.depthStencilAttachment.format = depthFormat;
    desc.depthStencilAttachment.sampleCount = 1;
    desc.depthStencilAttachment.loadOp = loadExisting ? ASH::LoadOp::Load : ASH::LoadOp::Clear;
    desc.depthStencilAttachment.storeOp = ASH::StoreOp::Store;
    desc.depthStencilAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    desc.depthStencilAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    m_renderPass = device->createRenderPass(desc);
}

void PointShadowRenderPass::cleanup()
{
    m_renderPass.reset();
}
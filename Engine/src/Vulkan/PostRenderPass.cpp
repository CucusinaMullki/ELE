#include "Vulkan/PostRenderPass.h"

void PostRenderPass::initialize(ASH::Device* device, ASH::Format colorFormat, ASH::LoadOp loadOp)
{
    ASH::AttachmentDesc colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.sampleCount = 1;
    colorAttachment.loadOp = loadOp;
    colorAttachment.storeOp = ASH::StoreOp::Store;
    colorAttachment.stencilLoadOp = ASH::LoadOp::DontCare;
    colorAttachment.stencilStoreOp = ASH::StoreOp::DontCare;

    ASH::RenderPassDesc desc{};
    desc.colorAttachments.push_back(colorAttachment);

    m_renderPass = device->createRenderPass(desc);
}

void PostRenderPass::cleanup()
{
    m_renderPass.reset();
}
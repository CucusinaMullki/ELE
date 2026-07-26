#include "Vulkan/PostRenderPass.h"

void PostRenderPass::initialize(vk::Device device, vk::Format colorFormat, vk::AttachmentLoadOp loadOp)
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(colorFormat);
    colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colorAttachment.setLoadOp(loadOp); 
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colorAttachment.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::AttachmentReference colorRef{};
    colorRef.setAttachment(0);
    colorRef.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass{};
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachments(colorRef);

    std::array<vk::SubpassDependency, 2> dependencies{};
    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[0].setDstSubpass(0);
    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[1].setSrcSubpass(0);
    dependencies[1].setDstSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachments(colorAttachment);
    renderPassInfo.setSubpasses(subpass);
    renderPassInfo.setDependencies(dependencies);

    m_renderPass = device.createRenderPassUnique(renderPassInfo);
}

void PostRenderPass::cleanup()
{
    m_renderPass.reset();
}
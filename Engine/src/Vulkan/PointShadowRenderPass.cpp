#include "Vulkan/PointShadowRenderPass.h"
#include <array>

void PointShadowRenderPass::initialize(vk::Device device, vk::Format depthFormat)
{
    vk::AttachmentDescription depthAttachment{};
    depthAttachment.setFormat(depthFormat);
    depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    depthAttachment.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::AttachmentReference depthRef{};
    depthRef.setAttachment(0);
    depthRef.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass{};
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachmentCount(0);
    subpass.setPDepthStencilAttachment(&depthRef);

    std::array<vk::SubpassDependency, 2> dependencies{};
    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[0].setDstSubpass(0);
    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests);
    dependencies[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[1].setSrcSubpass(0);
    dependencies[1].setDstSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests);
    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachments(depthAttachment);
    renderPassInfo.setSubpasses(subpass);
    renderPassInfo.setDependencies(dependencies);

    m_renderPass = device.createRenderPassUnique(renderPassInfo);
}

void PointShadowRenderPass::cleanup()
{
    m_renderPass.reset();
}
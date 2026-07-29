#include "Vulkan/HdrRenderPass.h"
#include <array>

void HdrRenderPass::initialize(vk::Device device, vk::Format colorFormat, vk::Format velocityFormat, vk::Format depthFormat)
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(colorFormat);
    colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colorAttachment.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::AttachmentDescription velocityAttachment{};
    velocityAttachment.setFormat(velocityFormat);
    velocityAttachment.setSamples(vk::SampleCountFlagBits::e1);
    velocityAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    velocityAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    velocityAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    velocityAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    velocityAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    velocityAttachment.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::AttachmentDescription depthAttachment{};
    depthAttachment.setFormat(depthFormat);
    depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    depthAttachment.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::AttachmentReference colorRef{};
    colorRef.setAttachment(0);
    colorRef.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
    
    vk::AttachmentReference velocityRef{};
    velocityRef.setAttachment(1);
    velocityRef.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
    
    vk::AttachmentReference depthRef{};
    depthRef.setAttachment(2);
    depthRef.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    
    std::array<vk::AttachmentReference, 2> colorRefs = { colorRef, velocityRef };
    
    vk::SubpassDescription subpass{};
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachments(colorRefs);
    subpass.setPDepthStencilAttachment(&depthRef);

    std::array<vk::SubpassDependency, 2> dependencies{};

    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[0].setDstSubpass(0);
    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | 
        vk::PipelineStageFlagBits::eEarlyFragmentTests);
    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | 
        vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[1].setSrcSubpass(0);
    dependencies[1].setDstSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | 
        vk::PipelineStageFlagBits::eLateFragmentTests);
    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | 
        vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);
    std::array<vk::AttachmentDescription, 3> attachments = { colorAttachment, velocityAttachment, depthAttachment };

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachments(attachments);
    renderPassInfo.setSubpasses(subpass);
    renderPassInfo.setDependencies(dependencies);

    m_renderPass = device.createRenderPassUnique(renderPassInfo);
}

void HdrRenderPass::cleanup()
{
    m_renderPass.reset();
}
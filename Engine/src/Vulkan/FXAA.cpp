#include "Vulkan/FXAA.h"

void FXAA::initializeWithRenderPass(vk::Device device, vk::PipelineCache cache,
    vk::RenderPass renderPass, uint32_t frameCount)
{
    PipelineConfig config{};
    config.vertexShader = "fullscreen";
    config.fragmentShader = "fxaa";
    config.renderPass = renderPass;
    config.cache = cache;
    config.frameCount = frameCount;
    config.postProcess = true;
    config.useDescriptors = false;
    config.useVertexInput = false;
    config.depthTest = false;
    config.depthWrite = false;
    config.cullMode = vk::CullModeFlagBits::eNone;
    config.colorAttachmentCount = 1;
    m_pipeline.initialize(device, config);

    std::vector<vk::DescriptorSetLayout> layouts(frameCount, m_pipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(m_pipeline.getDescriptorPool());
    allocInfo.setSetLayouts(layouts);
    m_descriptorSets = device.allocateDescriptorSets(allocInfo);
}

void FXAA::updateDescriptorSet(vk::Device device, uint32_t frameIndex, vk::ImageView srcView, vk::Sampler srcSampler)
{
    vk::DescriptorImageInfo info{};
    info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    info.setImageView(srcView);
    info.setSampler(srcSampler);

    vk::WriteDescriptorSet write{};
    write.setDstSet(m_descriptorSets[frameIndex]);
    write.setDstBinding(0);
    write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    write.setDescriptorCount(1);
    write.setImageInfo(info);

    device.updateDescriptorSets(write, nullptr);
}

void FXAA::cleanup()
{
    m_pipeline.cleanup();
}

void FXAA::apply(vk::CommandBuffer cmd, uint32_t frameIndex, vk::ImageView srcView, vk::Sampler srcSampler)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipeline());
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline.getLayout(), 0, m_descriptorSets[frameIndex], nullptr);
    cmd.draw(3, 1, 0, 0);
}
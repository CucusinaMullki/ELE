#include "Vulkan/FXAA.h"

void FXAA::initializeWithRenderPass(ASH::Device* device, ASH::RenderPass* renderPass, uint32_t frameCount)
{
    PipelineConfig config{};
    config.vertexShader = "fullscreen";
    config.fragmentShader = "fxaa";
    config.renderPass = renderPass;
    config.frameCount = frameCount;
    config.layoutType = LayoutType::PostProcess;
    config.useVertexInput = false;
    config.depthTest = false;
    config.depthWrite = false;
    config.cullMode = ASH::CullMode::None;
    config.colorAttachmentCount = 1;
    m_pipeline.initialize(device, config);

    m_descriptorSets.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; i++)
        m_descriptorSets[i] = m_pipeline.createDescriptorSet();
}

void FXAA::updateDescriptorSet(uint32_t frameIndex, ASH::Texture* srcTexture, ASH::Sampler* srcSampler)
{
    ASH::DescriptorImageInfo info{ srcTexture, srcSampler };
    ASH::DescriptorWrite write{ 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &info };
    m_descriptorSets[frameIndex]->update(&write, 1);
}

void FXAA::cleanup()
{
    m_pipeline.cleanup();
}

void FXAA::apply(ASH::CommandBuffer* cmd, uint32_t frameIndex, ASH::Texture* srcTexture, ASH::Sampler* srcSampler)
{
    updateDescriptorSet(frameIndex, srcTexture, srcSampler);

    cmd->bindPipeline(m_pipeline.getPipeline());
    cmd->bindDescriptorSet(m_pipeline.getPipeline(), 0, m_descriptorSets[frameIndex].get());
    cmd->draw(3, 1, 0, 0);
}
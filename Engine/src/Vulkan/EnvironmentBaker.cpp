#include "Vulkan/EnvironmentBaker.h"
#include <glm/gtc/matrix_transform.hpp>

void EnvironmentBaker::initialize(ASH::Device* device)
{
    if (m_initialized) return;
    m_initialized = true;

    m_cubeMesh.initialize(device);

    m_bakeRenderPass.initialize(device, ASH::Format::R32G32B32A32_Float, ASH::LoadOp::Clear);

    PipelineConfig equirectConfig{};
    equirectConfig.vertexShader = "equirect_to_cubemap";
    equirectConfig.fragmentShader = "equirect_to_cubemap";
    equirectConfig.renderPass = m_bakeRenderPass.getRenderPass();
    equirectConfig.frameCount = 1;
    equirectConfig.layoutType = LayoutType::SingleTexture;
    equirectConfig.colorAttachmentCount = 1;
    equirectConfig.depthTest = false;
    equirectConfig.depthWrite = false;
    equirectConfig.cullMode = ASH::CullMode::None;
    equirectConfig.useVertexInput = true;
    equirectConfig.pushConstantSize = sizeof(glm::mat4);
    equirectConfig.pushStage = ASH::ShaderStage::Vertex;
    m_equirectToCubePipeline.initialize(device, equirectConfig);

    PipelineConfig irradianceConfig = equirectConfig;
    irradianceConfig.vertexShader = "equirect_to_cubemap";
    irradianceConfig.fragmentShader = "irradiance_convolution";
    m_irradiancePipeline.initialize(device, irradianceConfig);
}

void EnvironmentBaker::renderCubeFacesWithPipeline(ASH::Device* device, ASH::Texture* srcTexture, ASH::Sampler* srcSampler,
    CubeTexture& target, uint32_t mipLevel, uint32_t faceSize, Pipeline& pipeline)
{
    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    std::array<glm::mat4, 6> captureViews = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    };

    auto descSet = pipeline.createDescriptorSet();
    ASH::DescriptorImageInfo srcInfo{ srcTexture, srcSampler };
    ASH::DescriptorWrite write{ 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &srcInfo };
    descSet->update(&write, 1);

    for (uint32_t face = 0; face < 6; face++)
    {
        ASH::FramebufferDesc fbDesc{};
        fbDesc.renderPass = m_bakeRenderPass.getRenderPass();
        fbDesc.colorAttachments = { target.getTexture() };
        fbDesc.colorAttachmentLayers = { face };
        fbDesc.colorAttachmentMips = { mipLevel };
        fbDesc.extent = { faceSize, faceSize };
        fbDesc.layers = 1;
        auto fb = device->createFramebuffer(fbDesc);

        auto cmd = device->createCommandBuffer();
        cmd->begin();

        ASH::ClearColor clear{};
        clear.f32[0] = 0; clear.f32[1] = 0; clear.f32[2] = 0; clear.f32[3] = 1;

        ASH::RenderPassBeginInfo passInfo{};
        passInfo.renderPass = m_bakeRenderPass.getRenderPass();
        passInfo.framebuffer = fb.get();
        passInfo.renderArea = { 0, 0, faceSize, faceSize };
        passInfo.colorClearValues = &clear;
        passInfo.colorClearCount = 1;

        cmd->beginRenderPass(passInfo);
        cmd->bindPipeline(pipeline.getPipeline());

        ASH::Viewport vp{};
        vp.width = (float)faceSize; vp.height = (float)faceSize; vp.maxDepth = 1.0f;
        cmd->setViewport(vp);
        ASH::Rect2D sc{ 0, 0, faceSize, faceSize };
        cmd->setScissor(sc);

        cmd->bindDescriptorSet(pipeline.getPipeline(), 0, descSet.get());

        glm::mat4 viewProj = captureProj * captureViews[face];
        cmd->pushConstants(pipeline.getPipeline(), ASH::ShaderStage::Vertex, 0, sizeof(glm::mat4), &viewProj);

        m_cubeMesh.bind(cmd.get());
        m_cubeMesh.draw(cmd.get());

        cmd->endRenderPass();
        cmd->end();

        ASH::SubmitInfo submitInfo{};
        submitInfo.commandBuffer = cmd.get();
        device->submit(submitInfo);
        device->waitIdle();
    }

    {
        auto cmd = device->createCommandBuffer();
        cmd->begin();

        ASH::TextureBarrier tb{};
        tb.texture = target.getTexture();
        tb.oldState = ASH::ResourceState::ColorAttachment;
        tb.newState = ASH::ResourceState::ShaderReadOnly;
        cmd->barrier(&tb, 1, nullptr, 0);

        cmd->end();

        ASH::SubmitInfo submitInfo{};
        submitInfo.commandBuffer = cmd.get();
        device->submit(submitInfo);
        device->waitIdle();
    }
}

void EnvironmentBaker::bakeEnvironmentCubemap(ASH::Device* device, const std::string& hdrPath, CubeTexture& outCubemap)
{
    HdrTexture hdrSource;
    hdrSource.initialize(device, hdrPath);

    renderCubeFacesWithPipeline(device, hdrSource.getTexture(), hdrSource.getSampler(), outCubemap, 0, outCubemap.getSize(), m_equirectToCubePipeline);

    hdrSource.cleanup();
}

void EnvironmentBaker::bakeIrradianceMap(ASH::Device* device, CubeTexture& sourceEnvironment, CubeTexture& outIrradiance)
{
    renderCubeFacesWithPipeline(device, sourceEnvironment.getTexture(), sourceEnvironment.getSampler(),
        outIrradiance, 0, outIrradiance.getSize(), m_irradiancePipeline);
}

void EnvironmentBaker::cleanup()
{
    m_cubeMesh.cleanup();
    m_equirectToCubePipeline.cleanup();
    m_irradiancePipeline.cleanup();
    m_bakeRenderPass.cleanup();
}

#include "Vulkan/VoxelGI.h"
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Scene/Mesh.h"
#include "Scene/Material.h"
#include <glm/glm.hpp>

void VoxelGI::initialize(ASH::Device* device, uint32_t resolution, glm::vec3 gridMin, glm::vec3 gridSize)
{
    m_resolution = resolution;
    m_gridMin = gridMin;
    m_gridSize = gridSize;

    m_voxelTexture.initialize(device, resolution, ASH::Format::R8G8B8A8_UNorm);

    ComputePipelineConfig clearConfig{};
    clearConfig.computeShader = "clear_voxels";
    clearConfig.layoutType = LayoutType::VoxelClear;
    m_clearPipeline.initialize(device, clearConfig);

    m_clearDescriptorSet = m_clearPipeline.createDescriptorSet();
    ASH::DescriptorImageInfo voxelInfo{ m_voxelTexture.getTexture(), nullptr };
    ASH::DescriptorWrite write{ 0, ASH::DescriptorType::StorageImage, 0, nullptr, &voxelInfo };
    m_clearDescriptorSet->update(&write, 1);

    m_voxelizeRenderPass.initialize(device);

    PipelineConfig voxelizeConfig{};
    voxelizeConfig.vertexShader = "voxelize";
    voxelizeConfig.fragmentShader = "voxelize";
    voxelizeConfig.renderPass = m_voxelizeRenderPass.getRenderPass();
    voxelizeConfig.frameCount = 1;
    voxelizeConfig.layoutType = LayoutType::Voxelize;
    voxelizeConfig.colorAttachmentCount = 0;
    voxelizeConfig.depthTest = false;
    voxelizeConfig.depthWrite = false;
    voxelizeConfig.cullMode = ASH::CullMode::None;
    voxelizeConfig.useVertexInput = true;
    voxelizeConfig.pushConstantSize = sizeof(VoxelizePush);
    voxelizeConfig.pushStage = ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment;
    m_voxelizePipeline.initialize(device, voxelizeConfig);

    ASH::FramebufferDesc fbDesc{};
    fbDesc.renderPass = m_voxelizeRenderPass.getRenderPass();
    fbDesc.extent = { resolution, resolution };
    fbDesc.layers = 1;
    m_voxelizeFramebuffer = device->createFramebuffer(fbDesc);
}

void VoxelGI::clearVoxels(ASH::CommandBuffer* cmd)
{
    cmd->bindPipeline(m_clearPipeline.getPipeline());
    cmd->bindDescriptorSet(m_clearPipeline.getPipeline(), 0, m_clearDescriptorSet.get());

    uint32_t groups = m_resolution / 8;
    cmd->dispatch(groups, groups, groups);
}

void VoxelGI::voxelizeScene(ASH::CommandBuffer* cmd, const Scene& scene,
    ASH::Buffer* lightBuffer,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Buffer* shadowDataBuffer)
{
    bool sceneChanged = (scene.getVersion() != m_lastKnownSceneVersion);
    if (sceneChanged)
    {
        m_perObjectDescriptorSets.clear();
        m_lastKnownSceneVersion = scene.getVersion();
    }

    ASH::RenderPassBeginInfo passInfo{};
    passInfo.renderPass = m_voxelizeRenderPass.getRenderPass();
    passInfo.framebuffer = m_voxelizeFramebuffer.get();
    passInfo.renderArea = { 0, 0, m_resolution, m_resolution };

    cmd->beginRenderPass(passInfo);

    cmd->bindPipeline(m_voxelizePipeline.getPipeline());

    ASH::Viewport vp{};
    vp.width = (float)m_resolution;
    vp.height = (float)m_resolution;
    vp.maxDepth = 1.0f;
    cmd->setViewport(vp);
    ASH::Rect2D sc{ 0, 0, m_resolution, m_resolution };
    cmd->setScissor(sc);

    size_t objIndex = 0;
    for (const auto& obj : scene.getObjects())
    {
        if (!obj->mesh) continue;

        ASH::DescriptorSet* descSet = nullptr;

        if (sceneChanged || objIndex >= m_perObjectDescriptorSets.size())
        {
            m_perObjectDescriptorSets.push_back(m_voxelizePipeline.createDescriptorSet());
            descSet = m_perObjectDescriptorSets.back().get();
        }
        else
        {
            descSet = m_perObjectDescriptorSets[objIndex].get();
        }

        auto mat = obj->getMaterial(0);
        if (!mat) { objIndex++; continue; }

        ASH::DescriptorImageInfo albedoInfo{ mat->getAlbedoTexture(), mat->getAlbedoSampler() };
        ASH::DescriptorImageInfo voxelInfo{ m_voxelTexture.getTexture(), nullptr };
        ASH::DescriptorBufferInfo lightInfo{ lightBuffer, 0, lightBuffer->getSize() };
        ASH::DescriptorImageInfo shadowInfo{ shadowTexture, shadowSampler };
        ASH::DescriptorImageInfo shadowAtlasInfo{ shadowAtlasTexture, shadowAtlasSampler };
        ASH::DescriptorBufferInfo shadowDataInfo{ shadowDataBuffer, 0, shadowDataBuffer->getSize() };

        std::array<ASH::DescriptorWrite, 6> writes{};
        writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &albedoInfo };
        writes[1] = { 1, ASH::DescriptorType::StorageImage, 0, nullptr, &voxelInfo };
        writes[2] = { 2, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };
        writes[3] = { 3, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowInfo };
        writes[4] = { 4, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowAtlasInfo };
        writes[5] = { 5, ASH::DescriptorType::UniformBuffer, 0, &shadowDataInfo, nullptr };
        descSet->update(writes.data(), (uint32_t)writes.size());

        descSet->update(writes.data(), (uint32_t)writes.size());
        cmd->bindDescriptorSet(m_voxelizePipeline.getPipeline(), 0, descSet);

        VoxelizePush push{};
        push.model = obj->transform.getMatrix();
        push.voxelGridMin = glm::vec4(m_gridMin, 0.0f);
        push.voxelGridSize = glm::vec4(m_gridSize, 0.0f);
        cmd->pushConstants(m_voxelizePipeline.getPipeline(), ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment, 0, sizeof(VoxelizePush), &push);

        obj->mesh->bind(cmd);
        for (const auto& sm : obj->mesh->getSubmeshes())
            obj->mesh->drawSubmesh(cmd, sm);

        objIndex++;
    }

    cmd->endRenderPass();
}

void VoxelGI::cleanup()
{
    m_perObjectDescriptorSets.clear();
    m_clearDescriptorSet.reset();
    m_voxelizeFramebuffer.reset();
    m_voxelizeRenderPass.cleanup();
    m_voxelizePipeline.cleanup();
    m_clearPipeline.cleanup();
    m_voxelTexture.cleanup();
}
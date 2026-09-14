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

    uint32_t mipLevels = (uint32_t)std::floor(std::log2((float)resolution)) + 1;
    m_voxelTexture.initialize(device, resolution, ASH::Format::R8G8B8A8_UNorm, mipLevels);

    ComputePipelineConfig clearConfig{};
    clearConfig.computeShader = "clear_voxels";
    clearConfig.layoutType = LayoutType::VoxelClear;
    m_clearPipeline.initialize(device, clearConfig);

    m_clearDescriptorSet = m_clearPipeline.createDescriptorSet();
    ASH::DescriptorImageInfo voxelInfo{ m_voxelTexture.getTexture(), nullptr };
    ASH::DescriptorWrite write{ 0, ASH::DescriptorType::StorageImage, 0, nullptr, &voxelInfo };
    m_clearDescriptorSet->update(&write, 1);

    m_voxelizeRenderPass.initialize(device);

    ComputePipelineConfig stampConfig{};
    stampConfig.computeShader = "stamp_proxy";
    stampConfig.layoutType = LayoutType::StampProxy;
    stampConfig.pushConstantSize = sizeof(glm::ivec4) * 2;
    stampConfig.pushStage = ASH::ShaderStage::Compute;
    m_stampPipeline.initialize(device, stampConfig);

    ComputePipelineConfig downsampleConfig{};
    downsampleConfig.computeShader  = "downsample_voxels";
    downsampleConfig.layoutType = LayoutType::DownsampleVoxels;
    m_downsamplePipeline.initialize(device, downsampleConfig);

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

    PipelineConfig voxelizeSkinnedConfig = voxelizeConfig;
    voxelizeSkinnedConfig.vertexShader = "voxelize_skinned";
    voxelizeSkinnedConfig.fragmentShader = "voxelize";
    voxelizeSkinnedConfig.layoutType = LayoutType::VoxelizeSkinned;
    m_voxelizeSkinnedPipeline.initialize(device, voxelizeSkinnedConfig);

    ASH::FramebufferDesc fbDesc{};
    fbDesc.renderPass = m_voxelizeRenderPass.getRenderPass();
    fbDesc.extent = { resolution, resolution };
    fbDesc.layers = 1;
    m_voxelizeFramebuffer = device->createFramebuffer(fbDesc);
}

void VoxelGI::buildMipChain(ASH::CommandBuffer* cmd)
{
    uint32_t mipLevels = m_voxelTexture.getMipLevels();

    if (m_downsampleDescriptorSets.empty())
    {
        m_downsampleDescriptorSets.resize(mipLevels - 1);
        for (uint32_t mip = 0; mip < mipLevels - 1; mip++)
        {
            m_downsampleDescriptorSets[mip] = m_downsamplePipeline.createDescriptorSet();

            ASH::DescriptorImageInfo srcInfo{};
            srcInfo.texture = m_voxelTexture.getTexture();
            srcInfo.mipLevel = mip;
            srcInfo.useSpecificMipLayer = true;

            ASH::DescriptorImageInfo dstInfo{};
            dstInfo.texture = m_voxelTexture.getTexture();
            dstInfo.mipLevel = mip + 1;
            dstInfo.useSpecificMipLayer = true;

            std::array<ASH::DescriptorWrite, 2> writes{};
            writes[0] = { 0, ASH::DescriptorType::StorageImage, 0, nullptr, &srcInfo };
            writes[1] = { 1, ASH::DescriptorType::StorageImage, 0, nullptr, &dstInfo };
            m_downsampleDescriptorSets[mip]->update(writes.data(), (uint32_t)writes.size());
        }
    }

    cmd->bindPipeline(m_downsamplePipeline.getPipeline());

    uint32_t srcSize = m_resolution;
    for (uint32_t mip = 0; mip < mipLevels - 1; mip++)
    {
        uint32_t dstSize = srcSize / 2;

        cmd->bindDescriptorSet(m_downsamplePipeline.getPipeline(), 0, m_downsampleDescriptorSets[mip].get());

        uint32_t groups = (dstSize + 7) / 8;
        cmd->dispatch(groups, groups, groups);

        srcSize = dstSize;
    }
}

void VoxelGI::bakeLocalProxySkinned(ASH::Device* device, SkeletalMesh* mesh, Material* material, DynamicVoxelProxy& proxy,
    ASH::Buffer* boneBuffer,
    ASH::Buffer* lightBuffer,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Buffer* shadowDataBuffer)
{
    uint32_t localRes = proxy.getResolution();

    ASH::FramebufferDesc fbDesc{};
    fbDesc.renderPass = m_voxelizeRenderPass.getRenderPass();
    fbDesc.extent = { localRes, localRes };
    fbDesc.layers = 1;
    auto localFb = device->createFramebuffer(fbDesc);

    auto cmd = device->createCommandBuffer();
    cmd->begin();

    ASH::TextureBarrier toGeneral{};
    toGeneral.texture = proxy.getLocalVoxelTexture().getTexture();
    toGeneral.oldState = ASH::ResourceState::Undefined;
    toGeneral.newState = ASH::ResourceState::General;
    cmd->barrier(&toGeneral, 1, nullptr, 0);

    ASH::RenderPassBeginInfo passInfo{};
    passInfo.renderPass = m_voxelizeRenderPass.getRenderPass();
    passInfo.framebuffer = localFb.get();
    passInfo.renderArea = { 0, 0, localRes, localRes };

    cmd->beginRenderPass(passInfo);
    cmd->bindPipeline(m_voxelizeSkinnedPipeline.getPipeline());

    ASH::Viewport vp{};
    vp.width = (float)localRes;
    vp.height = (float)localRes;
    vp.maxDepth = 1.0f;
    cmd->setViewport(vp);
    ASH::Rect2D sc{ 0, 0, localRes, localRes };
    cmd->setScissor(sc);

    auto descSet = m_voxelizeSkinnedPipeline.createDescriptorSet();

    ASH::DescriptorImageInfo albedoInfo{ material->getAlbedoTexture(), material->getAlbedoSampler() };
    ASH::DescriptorImageInfo voxelInfo{ proxy.getLocalVoxelTexture().getTexture(), nullptr };
    ASH::DescriptorBufferInfo lightInfo{ lightBuffer, 0, lightBuffer->getSize() };
    ASH::DescriptorImageInfo shadowInfo{ shadowTexture, shadowSampler };
    ASH::DescriptorImageInfo shadowAtlasInfo{ shadowAtlasTexture, shadowAtlasSampler };
    ASH::DescriptorBufferInfo shadowDataInfo{ shadowDataBuffer, 0, shadowDataBuffer->getSize() };
    ASH::DescriptorBufferInfo boneInfo{ boneBuffer, 0, boneBuffer->getSize() };

    std::array<ASH::DescriptorWrite, 7> writes{};
    writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &albedoInfo };
    writes[1] = { 1, ASH::DescriptorType::StorageImage, 0, nullptr, &voxelInfo };
    writes[2] = { 2, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };
    writes[3] = { 3, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowInfo };
    writes[4] = { 4, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowAtlasInfo };
    writes[5] = { 5, ASH::DescriptorType::UniformBuffer, 0, &shadowDataInfo, nullptr };
    writes[6] = { 6, ASH::DescriptorType::UniformBuffer, 0, &boneInfo, nullptr };
    descSet->update(writes.data(), (uint32_t)writes.size());

    cmd->bindDescriptorSet(m_voxelizeSkinnedPipeline.getPipeline(), 0, descSet.get());

    VoxelizePush push{};
    push.model = glm::mat4(1.0f);
    push.voxelGridMin = glm::vec4(proxy.getLocalBoundsMin(), 0.0f);
    push.voxelGridSize = glm::vec4(proxy.getLocalBoundsSize(), 0.0f);
    cmd->pushConstants(m_voxelizeSkinnedPipeline.getPipeline(), ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment, 0, sizeof(VoxelizePush), &push);

    mesh->bind(cmd.get());
    for (const auto& sm : mesh->getSubmeshes())
        mesh->drawSubmesh(cmd.get(), sm);

    cmd->endRenderPass();

    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();
}

void VoxelGI::stampProxy(ASH::CommandBuffer* cmd, DynamicVoxelProxy& proxy, glm::vec3 worldPosition)
{
    float voxelSize = m_gridSize.x / (float)m_resolution;
    glm::vec3 proxyWorldMin = worldPosition + proxy.getLocalBoundsMin();
    glm::ivec3 worldOffset = glm::ivec3(glm::floor((proxyWorldMin - m_gridMin) / voxelSize));

    if (!proxy.stampDescriptorSet)
    {
        proxy.stampDescriptorSet = m_stampPipeline.createDescriptorSet();

        ASH::DescriptorImageInfo localInfo{ proxy.getLocalVoxelTexture().getTexture(), nullptr };
        ASH::DescriptorImageInfo worldInfo{ m_voxelTexture.getTexture(), nullptr };

        std::array<ASH::DescriptorWrite, 2> writes{};
        writes[0] = { 0, ASH::DescriptorType::StorageImage, 0, nullptr, &localInfo };
        writes[1] = { 1, ASH::DescriptorType::StorageImage, 0, nullptr, &worldInfo };
        proxy.stampDescriptorSet->update(writes.data(), (uint32_t)writes.size());
    }

    cmd->bindPipeline(m_stampPipeline.getPipeline());
    cmd->bindDescriptorSet(m_stampPipeline.getPipeline(), 0, proxy.stampDescriptorSet.get());

    struct StampPush { glm::ivec4 worldOffset; glm::ivec4 localSize; } push{};
    push.worldOffset = glm::ivec4(worldOffset, 0);
    push.localSize = glm::ivec4((int)proxy.getResolution(), (int)proxy.getResolution(), (int)proxy.getResolution(), 0);
    cmd->pushConstants(m_stampPipeline.getPipeline(), ASH::ShaderStage::Compute, 0, sizeof(StampPush), &push);

    uint32_t groups = (proxy.getResolution() + 7) / 8;
    cmd->dispatch(groups, groups, groups);
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

std::unique_ptr<DynamicVoxelProxy> VoxelGI::createLocalProxy(ASH::Device* device, Mesh* mesh, Material* material,
    ASH::Buffer* lightBuffer,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Buffer* shadowDataBuffer)
{
    glm::vec3 boundsMin = mesh->getLocalBoundsMin();
    glm::vec3 boundsMax = mesh->getLocalBoundsMax();
    glm::vec3 boundsSize = boundsMax - boundsMin;

    float voxelSize = m_gridSize.x / (float)m_resolution;
    uint32_t localRes = (uint32_t)glm::ceil(glm::max(boundsSize.x, glm::max(boundsSize.y, boundsSize.z)) / voxelSize);
    localRes = glm::max(localRes, 1u);

    auto proxy = std::make_unique<DynamicVoxelProxy>();
    proxy->initialize(device, localRes, boundsMin, boundsSize);
    bakeLocalProxy(device, mesh, material, *proxy, lightBuffer, shadowTexture, shadowSampler, shadowAtlasTexture, shadowAtlasSampler, shadowDataBuffer);

    return proxy;
}

void VoxelGI::bakeLocalProxy(ASH::Device* device, Mesh* mesh, Material* material, DynamicVoxelProxy& proxy,
    ASH::Buffer* lightBuffer,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Buffer* shadowDataBuffer)
{
    uint32_t localRes = proxy.getResolution();

    ASH::FramebufferDesc fbDesc{};
    fbDesc.renderPass = m_voxelizeRenderPass.getRenderPass();
    fbDesc.extent = { localRes, localRes };
    fbDesc.layers = 1;
    auto localFb = device->createFramebuffer(fbDesc);

    auto cmd = device->createCommandBuffer();
    cmd->begin();

    ASH::TextureBarrier toGeneral{};
    toGeneral.texture = proxy.getLocalVoxelTexture().getTexture();
    toGeneral.oldState = ASH::ResourceState::Undefined;
    toGeneral.newState = ASH::ResourceState::General;
    cmd->barrier(&toGeneral, 1, nullptr, 0);

    ASH::RenderPassBeginInfo passInfo{};
    passInfo.renderPass = m_voxelizeRenderPass.getRenderPass();
    passInfo.framebuffer = localFb.get();
    passInfo.renderArea = { 0, 0, localRes, localRes };

    cmd->beginRenderPass(passInfo);
    cmd->bindPipeline(m_voxelizePipeline.getPipeline());

    ASH::Viewport vp{};
    vp.width = (float)localRes;
    vp.height = (float)localRes;
    vp.maxDepth = 1.0f;
    cmd->setViewport(vp);
    ASH::Rect2D sc{ 0, 0, localRes, localRes };
    cmd->setScissor(sc);

    auto descSet = m_voxelizePipeline.createDescriptorSet();

    ASH::DescriptorImageInfo albedoInfo{ material->getAlbedoTexture(), material->getAlbedoSampler() };
    ASH::DescriptorImageInfo voxelInfo{ proxy.getLocalVoxelTexture().getTexture(), nullptr };
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

    cmd->bindDescriptorSet(m_voxelizePipeline.getPipeline(), 0, descSet.get());

    VoxelizePush push{};
    push.model = glm::mat4(1.0f);
    push.voxelGridMin = glm::vec4(proxy.getLocalBoundsMin(), 0.0f);
    push.voxelGridSize = glm::vec4(proxy.getLocalBoundsSize(), 0.0f);
    cmd->pushConstants(m_voxelizePipeline.getPipeline(), ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment, 0, sizeof(VoxelizePush), &push);

    mesh->bind(cmd.get());
    for (const auto& sm : mesh->getSubmeshes())
        mesh->drawSubmesh(cmd.get(), sm);

    cmd->endRenderPass();

    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();
}

void VoxelGI::cleanup()
{
    m_voxelizeSkinnedPipeline.cleanup();
    m_downsamplePipeline.cleanup();
    m_stampPipeline.cleanup();
    m_voxelizeSkinnedPipeline.cleanup();
    m_stampDescriptorSet.reset();
    m_perObjectDescriptorSets.clear();
    m_clearDescriptorSet.reset();
    m_voxelizeFramebuffer.reset();
    m_voxelizeRenderPass.cleanup();
    m_voxelizePipeline.cleanup();
    m_clearPipeline.cleanup();
    m_voxelTexture.cleanup();
}
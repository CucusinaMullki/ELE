#include "Vulkan/VoxelGI.h"
#include "Scene/Scene.h"
#include "Core/Log.h"
#include "Scene/GameObject.h"
#include "Scene/Mesh.h"
#include "Scene/Material.h"
#include <cstring>

void VoxelGI::initialize(ASH::Device* device, float chunkSize, uint32_t chunkResolution, int32_t visibilityRadiusChunks)
{
    m_device = device;
    m_chunkResolution = chunkResolution;

    int32_t atlasSlotsPerAxis = visibilityRadiusChunks * 2 + 1;
    m_chunkManager.initialize(chunkSize, visibilityRadiusChunks, atlasSlotsPerAxis);

    uint32_t paddedResolution = chunkResolution + 2 * PADDING;
    uint32_t atlasResolution = (uint32_t)atlasSlotsPerAxis * paddedResolution;
    m_atlas.initialize(device, atlasResolution, ASH::Format::R8G8B8A8_UNorm, 1);

    {
        auto cmd = device->createCommandBuffer();
        cmd->begin();
        ASH::TextureBarrier tb{};
        tb.texture = m_atlas.getTexture();
        tb.oldState = ASH::ResourceState::Undefined;
        tb.newState = ASH::ResourceState::ShaderReadOnly;
        cmd->barrier(&tb, 1, nullptr, 0);
        cmd->end();

        ASH::SubmitInfo submitInfo{};
        submitInfo.commandBuffer = cmd.get();
        device->submit(submitInfo);
        device->waitIdle();
    }

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
    voxelizeConfig.pushConstantSize = sizeof(glm::mat4) + sizeof(glm::vec4) * 2 + sizeof(glm::ivec4);
    voxelizeConfig.pushStage = ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment;
    m_voxelizePipeline.initialize(device, voxelizeConfig);

    ComputePipelineConfig clearConfig{};
    clearConfig.computeShader = "clear_voxels";
    clearConfig.layoutType = LayoutType::VoxelClear;
    m_clearPipeline.initialize(device, clearConfig);

    int32_t pageTableSize = atlasSlotsPerAxis * atlasSlotsPerAxis * atlasSlotsPerAxis;
    ASH::BufferDesc pageTableDesc{};
    pageTableDesc.size = sizeof(int32_t) * pageTableSize;
    pageTableDesc.usage = ASH::BufferUsage::StorageBuffer;
    pageTableDesc.memory = ASH::MemoryUsage::CpuToGpu;
    m_pageTableBuffer = device->createBuffer(pageTableDesc);

    std::vector<int32_t> initialTable(pageTableSize, -1);
    void* mapped = m_pageTableBuffer->map();
    memcpy(mapped, initialTable.data(), initialTable.size() * sizeof(int32_t));
    m_pageTableBuffer->unmap();
}

void VoxelGI::updatePageTable(ASH::Device* device)
{
    int32_t radius = (m_chunkManager.getAtlasSlotsPerAxis() - 1) / 2;
    int32_t slotsPerAxis = m_chunkManager.getAtlasSlotsPerAxis();
    int32_t tableSize = slotsPerAxis * slotsPerAxis * slotsPerAxis;

    std::vector<int32_t> table(tableSize, -1);

    for (const auto& [coord, chunk] : m_chunkManager.getActiveChunks())
    {
        int32_t rx = coord.x - m_lastCameraChunk.x + radius;
        int32_t ry = coord.y - m_lastCameraChunk.y + radius;
        int32_t rz = coord.z - m_lastCameraChunk.z + radius;

        if (rx < 0 || rx >= slotsPerAxis || ry < 0 || ry >= slotsPerAxis || rz < 0 || rz >= slotsPerAxis)
            continue;

        int32_t index = rx + ry * slotsPerAxis + rz * slotsPerAxis * slotsPerAxis;
        table[index] = chunk.atlasSlotIndex;
    }

    void* mapped = m_pageTableBuffer->map();
    memcpy(mapped, table.data(), table.size() * sizeof(int32_t));
    m_pageTableBuffer->unmap();
}

void VoxelGI::update(ASH::CommandBuffer* cmd, const Scene& scene, glm::vec3 cameraPosition,
    ASH::Buffer* lightBuffer,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Buffer* shadowDataBuffer)
{
    m_lastCameraChunk = m_chunkManager.worldToChunkCoord(cameraPosition);

    auto result = m_chunkManager.update(cameraPosition);

    if (!result.chunksToBake.empty())
    {
        ChunkCoord coord = result.chunksToBake[0];
        LOG("Voxel GI: baking chunk (" << coord.x << "," << coord.y << "," << coord.z << ")");
        VoxelChunk* chunk = m_chunkManager.getChunk(coord);
        if (chunk)
        {
            bakeChunk(cmd, coord, chunk->atlasSlotIndex, scene, lightBuffer,
                shadowTexture, shadowSampler, shadowAtlasTexture, shadowAtlasSampler, shadowDataBuffer);
        }
    }

    if (!result.chunksToBake.empty() || !result.chunksToEvict.empty())
        updatePageTable(m_device);
}

void VoxelGI::bakeChunk(ASH::CommandBuffer* cmd, ChunkCoord coord, int32_t atlasSlot, const Scene& scene,
    ASH::Buffer* lightBuffer,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Buffer* shadowDataBuffer)
{
    m_bakeDescriptorSets.clear();

    float chunkSize = m_chunkManager.getChunkSize();
    glm::vec3 chunkWorldMin = m_chunkManager.chunkCoordToWorldMin(coord);
    glm::vec3 chunkWorldSize(chunkSize, chunkSize, chunkSize);

    int32_t slotsPerAxis = m_chunkManager.getAtlasSlotsPerAxis();
    int32_t slotZ = atlasSlot / (slotsPerAxis * slotsPerAxis);
    int32_t slotY = (atlasSlot / slotsPerAxis) % slotsPerAxis;
    int32_t slotX = atlasSlot % slotsPerAxis;


    uint32_t paddedResolution = m_chunkResolution + 2 * PADDING;

    uint32_t atlasOffsetX = (uint32_t)slotX * paddedResolution;
    uint32_t atlasOffsetY = (uint32_t)slotY * paddedResolution;
    uint32_t atlasOffsetZ = (uint32_t)slotZ * paddedResolution;

    if (!m_chunkFramebuffer)
    {
        ASH::FramebufferDesc fbDesc{};
        fbDesc.renderPass = m_voxelizeRenderPass.getRenderPass();
        fbDesc.extent = { m_chunkResolution, paddedResolution };
        fbDesc.layers = 1;
        m_chunkFramebuffer = m_device->createFramebuffer(fbDesc);
    }

    ASH::TextureBarrier toGeneral{};
    toGeneral.texture = m_atlas.getTexture();
    toGeneral.oldState = ASH::ResourceState::ShaderReadOnly;
    toGeneral.newState = ASH::ResourceState::General;
    cmd->barrier(&toGeneral, 1, nullptr, 0);

    ASH::RenderPassBeginInfo passInfo{};
    passInfo.renderPass = m_voxelizeRenderPass.getRenderPass();
    passInfo.framebuffer = m_chunkFramebuffer.get();
    passInfo.renderArea = { 0, 0, m_chunkResolution, paddedResolution };

    cmd->beginRenderPass(passInfo);
    cmd->bindPipeline(m_voxelizePipeline.getPipeline());

    ASH::Viewport vp{};
    vp.width = (float)paddedResolution;
    vp.height = (float)paddedResolution;
    vp.maxDepth = 1.0f;
    cmd->setViewport(vp);
    ASH::Rect2D sc{ 0, 0, paddedResolution, paddedResolution };
    cmd->setScissor(sc);

    for (const auto& obj : scene.getObjects())
    {
        if (!obj->mesh) continue;

        obj->mesh->bind(cmd);

        for (const auto& sm : obj->mesh->getSubmeshes())
        {
            auto mat = obj->getMaterial(sm.materialIndex);
            if (!mat) continue;

            m_bakeDescriptorSets.push_back(m_voxelizePipeline.createDescriptorSet());
            ASH::DescriptorSet* descSet = m_bakeDescriptorSets.back().get();

            ASH::DescriptorImageInfo albedoInfo{ mat->getAlbedoTexture(), mat->getAlbedoSampler() };
            ASH::DescriptorImageInfo voxelInfo{};
            voxelInfo.texture = m_atlas.getTexture();
            voxelInfo.useSpecificMipLayer = true;
            voxelInfo.mipLevel = 0;

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

            cmd->bindDescriptorSet(m_voxelizePipeline.getPipeline(), 0, descSet);

            struct ChunkVoxelizePush
            {
                glm::mat4 model;
                glm::vec4 gridMin;
                glm::vec4 gridSize;
                glm::ivec4 atlasOffset;
            } push{};

            float voxelWorldSize = chunkSize / (float)m_chunkResolution;
            float paddingWorld = voxelWorldSize * (float)PADDING;
            glm::vec3 expandedMin = chunkWorldMin - glm::vec3(paddingWorld);
            glm::vec3 expandedSize = chunkWorldSize + glm::vec3(paddingWorld * 2.0f);

            push.model = obj->transform.getMatrix();
            push.gridMin = glm::vec4(expandedMin, 0.0f);
            push.gridSize = glm::vec4(expandedSize, 0.0f);
            push.atlasOffset = glm::ivec4((int)atlasOffsetX, (int)atlasOffsetY, (int)atlasOffsetZ, (int)paddedResolution);
            cmd->pushConstants(m_voxelizePipeline.getPipeline(), ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment, 0, sizeof(ChunkVoxelizePush), &push);

            obj->mesh->drawSubmesh(cmd, sm);
        }
    }

    cmd->endRenderPass();

    ASH::TextureBarrier toShaderRead{};
    toShaderRead.texture = m_atlas.getTexture();
    toShaderRead.oldState = ASH::ResourceState::General;
    toShaderRead.newState = ASH::ResourceState::ShaderReadOnly;
    cmd->barrier(&toShaderRead, 1, nullptr, 0);
}

void VoxelGI::cleanup()
{
    m_chunkFramebuffer.reset();
    m_bakeDescriptorSets.clear();
    m_pageTableBuffer.reset();
    m_clearPipeline.cleanup();
    m_voxelizePipeline.cleanup();
    m_voxelizeRenderPass.cleanup();
    m_atlas.cleanup();
}

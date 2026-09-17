#pragma once
#include "ASH/ASH.h"
#include "Vulkan/VoxelTexture3D.h"
#include "Vulkan/VoxelChunkManager.h"
#include "Vulkan/ComputePipeline.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/VoxelizeRenderPass.h"
#include <memory>
#include <glm/glm.hpp>

class Scene;

class VoxelGI
{
public:
    void initialize(ASH::Device* device, float chunkSize, uint32_t chunkResolution,
        int32_t visibilityRadiusChunks);
    void cleanup();

    void update(ASH::CommandBuffer* cmd, const Scene& scene, glm::vec3 cameraPosition,
        ASH::Buffer* lightBuffer,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        ASH::Buffer* shadowDataBuffer);

    ASH::Texture* getAtlasTexture() const { return m_atlas.getTexture(); }
    ASH::Sampler* getAtlasSampler() const { return m_atlas.getSampler(); }
    ChunkCoord getCameraChunkCoord() const { return m_lastCameraChunk; }
    ASH::Buffer* getPageTableBuffer() const { return m_pageTableBuffer.get(); }

    float getChunkSize() const { return m_chunkManager.getChunkSize(); }
    uint32_t getChunkResolution() const { return m_chunkResolution; }
    int32_t getAtlasSlotsPerAxis() const { return m_chunkManager.getAtlasSlotsPerAxis(); }

private:
    void bakeChunk(ASH::CommandBuffer* cmd, ChunkCoord coord, int32_t atlasSlot, const Scene& scene,
        ASH::Buffer* lightBuffer,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        ASH::Buffer* shadowDataBuffer);
    void updatePageTable(ASH::Device* device);

    static constexpr uint32_t PADDING = 1;
    
    VoxelChunkManager m_chunkManager;
    VoxelTexture3D m_atlas;
    uint32_t m_chunkResolution = 32;

    VoxelizeRenderPass m_voxelizeRenderPass;
    Pipeline m_voxelizePipeline;
    std::unique_ptr<ASH::Framebuffer> m_chunkFramebuffer;
    std::vector<std::unique_ptr<ASH::DescriptorSet>> m_bakeDescriptorSets;

    ComputePipeline m_clearPipeline;

    std::unique_ptr<ASH::Buffer> m_pageTableBuffer;
    ChunkCoord m_lastCameraChunk{ INT32_MIN, INT32_MIN, INT32_MIN };

    ASH::Device* m_device = nullptr;
};
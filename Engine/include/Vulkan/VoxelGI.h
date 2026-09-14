#pragma once
#include "ASH/ASH.h"
#include "Vulkan/VoxelTexture3D.h"
#include "Vulkan/ComputePipeline.h"
#include "Vulkan/VoxelizeRenderPass.h"
#include "Vulkan/DynamicVoxelProxy.h"
#include "Vulkan/RenderPass.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Scene;

struct VoxelizePush
{
    glm::mat4 model;
    glm::vec4 voxelGridMin;
    glm::vec4 voxelGridSize;
};

class VoxelGI
{
public:
    void initialize(ASH::Device* device, uint32_t resolution, glm::vec3 gridMin, glm::vec3 gridSize);
    void cleanup();

    void clearVoxels(ASH::CommandBuffer* cmd);

    void stampProxy(ASH::CommandBuffer* cmd, DynamicVoxelProxy& proxy, glm::vec3 worldPosition);

    void bakeLocalProxy(ASH::Device* device, Mesh* mesh, Material* material, DynamicVoxelProxy& proxy,
        ASH::Buffer* lightBuffer,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        ASH::Buffer* shadowDataBuffer);

    std::unique_ptr<DynamicVoxelProxy> createLocalProxy(ASH::Device* device, Mesh* mesh, Material* material,
        ASH::Buffer* lightBuffer,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        ASH::Buffer* shadowDataBuffer);

    void voxelizeScene(ASH::CommandBuffer* cmd, const Scene& scene,
        ASH::Buffer* lightBuffer,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        ASH::Buffer* shadowDataBuffer);

    ASH::Texture* getVoxelTexture() const { return m_voxelTexture.getTexture(); }
    ASH::Sampler* getVoxelSampler() const { return m_voxelTexture.getSampler(); }
    glm::vec3 getGridMin() const { return m_gridMin; }
    glm::vec3 getGridSize() const { return m_gridSize; }
    uint32_t getResolution() const { return m_resolution; }

private:
    VoxelTexture3D m_voxelTexture;
    ComputePipeline m_clearPipeline;
    ComputePipeline m_stampPipeline;
    std::unique_ptr<ASH::DescriptorSet> m_stampDescriptorSet;
    Pipeline m_voxelizePipeline;
    std::unique_ptr<ASH::DescriptorSet> m_clearDescriptorSet;
    VoxelizeRenderPass m_voxelizeRenderPass;
    std::unique_ptr<ASH::Framebuffer> m_voxelizeFramebuffer;
    std::vector<std::unique_ptr<ASH::DescriptorSet>> m_perObjectDescriptorSets;

    bool m_descriptorsInitialized = false;
    uint32_t m_lastKnownSceneVersion = 0xFFFFFFFF;

    uint32_t m_resolution = 0;
    glm::vec3 m_gridMin{};
    glm::vec3 m_gridSize{};
};
#pragma once
#include "ASH/ASH.h"
#include "Vulkan/Texture.h"
#include <string>
#include <vector>
#include <memory>

class Material
{
public:
    void load(ASH::Device* device,
        const std::string& albedoPath,
        const std::string& normalPath,
        const std::string& metallicPath,
        const std::string& roughnessPath);

    void loadFromFiles(ASH::Device* device,
        const std::string& albedoPath,
        const std::string& normalPath,
        const std::string& metallicRoughnessPath);

    void loadFromPixels(ASH::Device* device,
        const unsigned char* albedoPx, int aw, int ah,
        const unsigned char* normalPx, int nw, int nh,
        const unsigned char* mrPx, int mw, int mh,
        const unsigned char* aoPx, int ow, int oh,
        const unsigned char* lightmapPx, int lw, int lh, bool hasLightmap);

    ASH::Texture* getAlbedoTexture() const { return m_albedo.getTexture(); }
    ASH::Sampler* getAlbedoSampler() const { return m_albedo.getSampler(); }
    ASH::Texture* getNormalTexture() const { return m_normal.getTexture(); }
    ASH::Sampler* getNormalSampler() const { return m_normal.getSampler(); }
    ASH::Texture* getMetallicRoughnessTexture() const { return m_metallicRoughness.getTexture(); }
    ASH::Sampler* getMetallicRoughnessSampler() const { return m_metallicRoughness.getSampler(); }
    ASH::Texture* getAoTexture() const { return m_ao.getTexture(); }
    ASH::Sampler* getAoSampler() const { return m_ao.getSampler(); }
    ASH::Texture* getLightmapTexture() const { return m_lightmap.getTexture(); }
    ASH::Texture* albedoTexture() { return m_albedo.getTexture(); }
    ASH::Sampler* getLightmapSampler() const { return m_lightmap.getSampler(); }
    bool hasLightmap() const { return m_hasLightmap; }

    void writeRtMask(uint32_t frame, ASH::Texture* tex, ASH::Sampler* sampler);
    void writeRtAo(uint32_t frame, ASH::Texture* tex, ASH::Sampler* sampler);
    void writeRtGi(uint32_t frame, ASH::Texture* tex, ASH::Sampler* sampler);
    void writeRtReflection(uint32_t frame, ASH::Texture* tex, ASH::Sampler* sampler);

    void createDescriptorSet(ASH::Device* device, ASH::DescriptorSetLayout* layout,
        const std::vector<ASH::Buffer*>& uboBuffers,
        const std::vector<ASH::Buffer*>& lightBuffers,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        ASH::Texture* irradianceTexture, ASH::Sampler* irradianceSampler,
        ASH::Texture* voxelTexture, ASH::Sampler* voxelSampler, ASH::Buffer* voxelGridBuffer,
        ASH::Buffer* voxelGridInfoBuffer,
        ASH::Buffer* probeData, ASH::Buffer* probeInfo,
        uint32_t frameCount);


    void createSkinnedDescriptorSet(ASH::Device* device, ASH::DescriptorSetLayout* layout,
        const std::vector<ASH::Buffer*>& uboBuffers,
        const std::vector<ASH::Buffer*>& lightBuffers,
        ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
        ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
        const std::vector<ASH::Buffer*>& boneBuffers,
        ASH::Texture* irradianceTexture, ASH::Sampler* irradianceSampler,
        ASH::Buffer* probeData, ASH::Buffer* probeInfo,
        uint32_t frameCount);

    ASH::DescriptorSet* getDescriptorSet(uint32_t frame) const { return m_descriptorSets[frame].get(); }
    void cleanup();

private:
    Texture m_albedo;
    Texture m_normal;
    Texture m_metallicRoughness;
    Texture m_ao;
    Texture m_lightmap;
    bool m_hasLightmap = false;
    bool m_hasRtBinding = false;
    std::vector<std::unique_ptr<ASH::DescriptorSet>> m_descriptorSets;
};

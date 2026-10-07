#include "Scene/Material.h"
#include <array>
#include <stb_image.h>
#include <filesystem>
#include <cstdio>

void Material::load(ASH::Device* device,
    const std::string& albedoPath,
    const std::string& normalPath,
    const std::string& metallicPath,
    const std::string& roughnessPath)
{
    m_albedo.initialize(device, albedoPath);
    m_normal.initialize(device, normalPath, false);
    m_metallicRoughness.initialize(device, metallicPath, false);
    m_ao.initialize(device, roughnessPath, false);

    static const unsigned char black[4] = { 0, 0, 0, 255 };
    m_lightmap.initializeFromMemory(device, black, 1, 1, false);
}

void Material::createDescriptorSet(ASH::Device* device, ASH::DescriptorSetLayout* layout,
    const std::vector<ASH::Buffer*>& uboBuffers,
    const std::vector<ASH::Buffer*>& lightBuffers,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    ASH::Texture* irradianceTexture, ASH::Sampler* irradianceSampler,
    ASH::Texture* voxelTexture, ASH::Sampler* voxelSampler, ASH::Buffer* voxelGridBuffer,
    ASH::Buffer* voxelGridInfoBuffer,
    uint32_t frameCount)
{
    m_descriptorSets.clear();
    m_descriptorSets.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        m_descriptorSets[i] = device->createDescriptorSet(layout);

        ASH::DescriptorBufferInfo uboInfo{ uboBuffers[i], 0, uboBuffers[i]->getSize() };
        ASH::DescriptorBufferInfo lightInfo{ lightBuffers[i], 0, lightBuffers[i]->getSize() };

        ASH::DescriptorImageInfo albedoInfo{ m_albedo.getTexture(), m_albedo.getSampler() };
        ASH::DescriptorImageInfo normalInfo{ m_normal.getTexture(), m_normal.getSampler() };
        ASH::DescriptorImageInfo mrInfo{ m_metallicRoughness.getTexture(), m_metallicRoughness.getSampler() };
        ASH::DescriptorImageInfo aoInfo{ m_ao.getTexture(), m_ao.getSampler() };
        ASH::DescriptorImageInfo shadowInfo{ shadowTexture, shadowSampler };
        ASH::DescriptorImageInfo lightmapInfo{ m_lightmap.getTexture(), m_lightmap.getSampler() };
        ASH::DescriptorImageInfo shadowAtlasInfo{ shadowAtlasTexture, shadowAtlasSampler };
        ASH::DescriptorImageInfo irradianceInfo{ irradianceTexture, irradianceSampler };
        ASH::DescriptorImageInfo voxelInfo{ voxelTexture, voxelSampler };
        ASH::DescriptorBufferInfo voxelGridInfo{ voxelGridBuffer, 0, voxelGridBuffer->getSize() };
        ASH::DescriptorBufferInfo voxelInfoBufInfo{ voxelGridInfoBuffer, 0, voxelGridInfoBuffer->getSize() };

        std::array<ASH::DescriptorWrite, 13> writes{};

        writes[0] = { 0, ASH::DescriptorType::UniformBuffer, 0, &uboInfo, nullptr };
        writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &albedoInfo };
        writes[2] = { 2, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &normalInfo };
        writes[3] = { 3, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &mrInfo };
        writes[4] = { 4, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &aoInfo };
        writes[5] = { 5, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };
        writes[6] = { 6, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowInfo };
        writes[7] = { 7, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowAtlasInfo };
        writes[8] = { 8, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &lightmapInfo };
        writes[9] = { 9, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &irradianceInfo };
        writes[10] = {10, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &voxelInfo };
        writes[11] = { 11, ASH::DescriptorType::StorageBuffer, 0, &voxelGridInfo, nullptr };
        writes[12] = { 12, ASH::DescriptorType::UniformBuffer, 0, &voxelInfoBufInfo, nullptr };

        m_descriptorSets[i]->update(writes.data(), (uint32_t)writes.size());
    }
}

void Material::loadFromFiles(ASH::Device* device,
    const std::string& albedoPath,
    const std::string& normalPath,
    const std::string& metallicRoughnessPath)
{
    static const unsigned char white[4] = { 255, 255, 255, 255 };
    static const unsigned char normalPx[4] = { 128, 128, 255, 255 };
    static const unsigned char defaultMR[4] = { 0, 128, 0, 255 };
    static const unsigned char black[4] = { 0, 0, 0, 255 };

    auto loadSlot = [&](Texture& tex, const std::string& path, bool srgb,
        const unsigned char* fallback, bool halfBlue, const char* slot)
    {
        if (!path.empty())
        {
            std::filesystem::path k(path);
            k.replace_extension(".ktx2");
            if (std::filesystem::exists(k) && tex.initializeKtx2(device, k.string()))
                return;

            int w = 0, h = 0;
            unsigned char* px = stbi_load(path.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
            if (px)
            {
                if (halfBlue)
                {
                    size_t n = (size_t)w * (size_t)h;
                    for (size_t i = 0; i < n; i++) px[i * 4 + 2] = px[i * 4 + 2] / 2;
                }
                std::fprintf(stderr, "[Tex] PNG %s %s\n", slot, path.c_str());
                tex.initializeFromMemory(device, px, w, h, srgb);
                stbi_image_free(px);
                return;
            }
        }
        tex.initializeFromMemory(device, fallback, 1, 1, srgb);
    };

    loadSlot(m_albedo, albedoPath, true,  white, false, "albedo");
    loadSlot(m_normal, normalPath, false, normalPx, false, "normal");
    loadSlot(m_metallicRoughness, metallicRoughnessPath, false, defaultMR, true,  "mr");
    m_ao.initializeFromMemory(device, white, 1, 1, false);
    m_lightmap.initializeFromMemory(device, black, 1, 1, true);
    m_hasLightmap = false;
}

void Material::cleanup()
{
    m_descriptorSets.clear();
    m_albedo.cleanup();
    m_normal.cleanup();
    m_metallicRoughness.cleanup();
    m_ao.cleanup();
    m_lightmap.cleanup();
}

void Material::loadFromPixels(ASH::Device* device,
    const unsigned char* albedoPx, int aw, int ah,
    const unsigned char* normalPx, int nw, int nh,
    const unsigned char* mrPx, int mw, int mh,
    const unsigned char* aoPx, int ow, int oh,
    const unsigned char* lightmapPx, int lw, int lh, bool hasLightmap)
{
    m_albedo.initializeFromMemory(device, albedoPx, aw, ah, true);
    m_normal.initializeFromMemory(device, normalPx, nw, nh, false);
    m_metallicRoughness.initializeFromMemory(device, mrPx, mw, mh, false);
    m_ao.initializeFromMemory(device, aoPx, ow, oh, false);
    m_lightmap.initializeFromMemory(device, lightmapPx, lw, lh, true);
    m_hasLightmap = hasLightmap;
}

void Material::createSkinnedDescriptorSet(ASH::Device* device, ASH::DescriptorSetLayout* layout,
    const std::vector<ASH::Buffer*>& uboBuffers,
    const std::vector<ASH::Buffer*>& lightBuffers,
    ASH::Texture* shadowTexture, ASH::Sampler* shadowSampler,
    ASH::Texture* shadowAtlasTexture, ASH::Sampler* shadowAtlasSampler,
    const std::vector<ASH::Buffer*>& boneBuffers,
    ASH::Texture* irradianceTexture, ASH::Sampler* irradianceSampler,
    uint32_t frameCount)
{
    m_descriptorSets.clear();
    m_descriptorSets.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        m_descriptorSets[i] = device->createDescriptorSet(layout);

        ASH::DescriptorBufferInfo uboInfo{ uboBuffers[i], 0, uboBuffers[i]->getSize() };
        ASH::DescriptorBufferInfo lightInfo{ lightBuffers[i], 0, lightBuffers[i]->getSize() };
        ASH::DescriptorBufferInfo boneInfo{ boneBuffers[i], 0, boneBuffers[i]->getSize() };

        ASH::DescriptorImageInfo albedoInfo{ m_albedo.getTexture(), m_albedo.getSampler() };
        ASH::DescriptorImageInfo normalInfo{ m_normal.getTexture(), m_normal.getSampler() };
        ASH::DescriptorImageInfo mrInfo{ m_metallicRoughness.getTexture(), m_metallicRoughness.getSampler() };
        ASH::DescriptorImageInfo aoInfo{ m_ao.getTexture(), m_ao.getSampler() };
        ASH::DescriptorImageInfo shadowInfo{ shadowTexture, shadowSampler };
        ASH::DescriptorImageInfo lightmapInfo{ m_lightmap.getTexture(), m_lightmap.getSampler() };
        ASH::DescriptorImageInfo shadowAtlasInfo{ shadowAtlasTexture, shadowAtlasSampler };
        ASH::DescriptorImageInfo irradianceInfo{ irradianceTexture, irradianceSampler };

        std::array<ASH::DescriptorWrite, 11> writes{};

        writes[0] = { 0, ASH::DescriptorType::UniformBuffer, 0, &uboInfo, nullptr };
        writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &albedoInfo };
        writes[2] = { 2, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &normalInfo };
        writes[3] = { 3, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &mrInfo };
        writes[4] = { 4, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &aoInfo };
        writes[5] = { 5, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };
        writes[6] = { 6, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowInfo };
        writes[7] = { 7, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowAtlasInfo };
        writes[8] = { 8, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &lightmapInfo };
        writes[9] = { 9, ASH::DescriptorType::UniformBuffer, 0, &boneInfo, nullptr };
        writes[10] = { 10, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &irradianceInfo };

        m_descriptorSets[i]->update(writes.data(), (uint32_t)writes.size());
    }
}
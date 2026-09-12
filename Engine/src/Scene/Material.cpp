#include "Scene/Material.h"
#include <array>

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

        std::array<ASH::DescriptorWrite, 9> writes{};

        writes[0] = { 0, ASH::DescriptorType::UniformBuffer, 0, &uboInfo, nullptr };
        writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &albedoInfo };
        writes[2] = { 2, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &normalInfo };
        writes[3] = { 3, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &mrInfo };
        writes[4] = { 4, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &aoInfo };
        writes[5] = { 5, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };
        writes[6] = { 6, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowInfo };
        writes[7] = { 7, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &shadowAtlasInfo };
        writes[8] = { 8, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &lightmapInfo };

        m_descriptorSets[i]->update(writes.data(), (uint32_t)writes.size());
    }
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

        std::array<ASH::DescriptorWrite, 10> writes{};

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

        m_descriptorSets[i]->update(writes.data(), (uint32_t)writes.size());
    }
}
#include "Scene/ScreenSpaceDecal.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Vulkan/LightBuffer.h"
#include "Core/Log.h"

namespace
{
const unsigned char kWhitePixel[4] = {255, 255, 255, 255};
const unsigned char kFlatNormal[4] = {128, 128, 255, 255};
const unsigned char kDefaultMR[4] = {0, 128, 0, 255};
}

void ScreenSpaceDecal::loadTexture(ASH::Device* device,
    const std::string& albedoPath,
    const std::string& opacityPath,
    const std::string& normalPath,
    const std::string& mrPath)
{
    m_albedoTexture.initialize(device, albedoPath, true);

    if (!opacityPath.empty())
        m_opacityTexture.initialize(device, opacityPath, false);
    else
        m_opacityTexture.initializeFromMemory(device, kWhitePixel, 1, 1, false);

    if (!normalPath.empty())
        m_normalTexture.initialize(device, normalPath, false);
    else
        m_normalTexture.initializeFromMemory(device, kFlatNormal, 1, 1, false);

    if (!mrPath.empty())
        m_mrTexture.initialize(device, mrPath, false);
    else
        m_mrTexture.initializeFromMemory(device, kDefaultMR, 1, 1, false);
}

void ScreenSpaceDecal::cleanup()
{
    m_descriptorSets.clear();
    m_albedoTexture.cleanup();
    m_opacityTexture.cleanup();
    m_normalTexture.cleanup();
    m_mrTexture.cleanup();
}

glm::mat4 ScreenSpaceDecal::getModelMatrix() const
{
    glm::mat4 m(1.0f);
    m = glm::translate(m, position);
    m = glm::rotate(m, glm::radians(rotationEuler.x), glm::vec3(1,0,0));
    m = glm::rotate(m, glm::radians(rotationEuler.y), glm::vec3(0,1,0));
    m = glm::rotate(m, glm::radians(rotationEuler.z), glm::vec3(0,0,1));
    m = glm::scale(m, size);
    return m;
}

glm::mat4 ScreenSpaceDecal::getInvModelMatrix() const
{
    return glm::inverse(getModelMatrix());
}

void ScreenSpaceDecal::createDescriptorSet(ASH::Device* device, ASH::DescriptorSetLayout* layout,
    ASH::Texture* depthTexture, ASH::Sampler* depthSampler,
    const DecalCameraBuffer& cameraBuffer, const LightBuffer& lightBuffer, uint32_t frameCount)
{
    m_descriptorSets.clear();
    m_descriptorSets.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        m_descriptorSets[i] = device->createDescriptorSet(layout);

        ASH::DescriptorImageInfo decalTexInfo{ m_albedoTexture.getTexture(), m_albedoTexture.getSampler() };
        ASH::DescriptorImageInfo depthInfo{ depthTexture, depthSampler };
        ASH::DescriptorImageInfo opacityInfo{ m_opacityTexture.getTexture(), m_opacityTexture.getSampler() };
        ASH::DescriptorImageInfo normalInfo{ m_normalTexture.getTexture(), m_normalTexture.getSampler() };
        ASH::DescriptorImageInfo mrInfo{ m_mrTexture.getTexture(), m_mrTexture.getSampler() };

        ASH::DescriptorBufferInfo camInfo{ cameraBuffer.getBuffer(i), 0, cameraBuffer.getBuffer(i)->getSize() };
        ASH::DescriptorBufferInfo lightInfo{ lightBuffer.getBuffer(i), 0, lightBuffer.getBuffer(i)->getSize() };

        std::array<ASH::DescriptorWrite, 7> writes{};
        writes[0] = { 0, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &decalTexInfo };
        writes[1] = { 1, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &depthInfo };
        writes[2] = { 2, ASH::DescriptorType::UniformBuffer, 0, &camInfo, nullptr };
        writes[3] = { 3, ASH::DescriptorType::UniformBuffer, 0, &lightInfo, nullptr };
        writes[4] = { 4, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &opacityInfo };
        writes[5] = { 5, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &normalInfo };
        writes[6] = { 6, ASH::DescriptorType::CombinedImageSampler, 0, nullptr, &mrInfo };

        m_descriptorSets[i]->update(writes.data(), (uint32_t)writes.size());
    }
}
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

void ScreenSpaceDecal::loadTexture(vk::Device device, vk::PhysicalDevice physicalDevice,
    vk::CommandPool pool, vk::Queue queue, 
    const std::string& albedoPath,
    const std::string& opacityPath,
    const std::string& normalPath,
    const std::string& mrPath)
{
    m_albedoTexture.initialize(device, physicalDevice, pool, queue, albedoPath, true);

    if (!opacityPath.empty())
        m_opacityTexture.initialize(device, physicalDevice, pool, queue, opacityPath, false);
    else
        m_opacityTexture.initializeFromMemory(device, physicalDevice, pool, queue, kWhitePixel, 1, 1, false);

    if (!normalPath.empty())
        m_normalTexture.initialize(device, physicalDevice, pool, queue, normalPath, false);
    else
        m_normalTexture.initializeFromMemory(device, physicalDevice, pool, queue, kFlatNormal, 1, 1, false);

    if (!mrPath.empty())
        m_mrTexture.initialize(device, physicalDevice, pool, queue, mrPath, false);
    else 
        m_mrTexture.initializeFromMemory(device, physicalDevice, pool, queue, kDefaultMR, 1, 1, false);
}

void ScreenSpaceDecal::cleanup()
{
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

void ScreenSpaceDecal::createDescriptorSet(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout,
    vk::ImageView depthView, vk::Sampler depthSampler,
    const DecalCameraBuffer& cameraBuffer, const LightBuffer& lightBuffer, uint32_t frameCount)
{
    LOG("decal depth view  = " << (void*)(VkImageView)depthView);
    std::vector<vk::DescriptorSetLayout> layouts(frameCount, layout);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(pool);
    allocInfo.setSetLayouts(layouts);
    m_descriptorSets = device.allocateDescriptorSets(allocInfo);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        vk::DescriptorImageInfo decalTexInfo{};
        decalTexInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        decalTexInfo.setImageView(m_albedoTexture.getImageView());
        decalTexInfo.setSampler(m_albedoTexture.getSampler());

        vk::DescriptorImageInfo depthInfo{};
        depthInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        depthInfo.setImageView(depthView);
        depthInfo.setSampler(depthSampler);

        std::array<vk::WriteDescriptorSet, 7> writes{};
        writes[0].setDstSet(m_descriptorSets[i]);
        writes[0].setDstBinding(0);
        writes[0].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        writes[0].setDescriptorCount(1);
        writes[0].setImageInfo(decalTexInfo);

        writes[1].setDstSet(m_descriptorSets[i]);
        writes[1].setDstBinding(1);
        writes[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        writes[1].setDescriptorCount(1);
        writes[1].setImageInfo(depthInfo);

        writes[2].setDstSet(m_descriptorSets[i]);
        writes[2].setDstBinding(2);
        writes[2].setDescriptorType(vk::DescriptorType::eUniformBuffer);
        writes[2].setDescriptorCount(1);
        vk::DescriptorBufferInfo camInfo = cameraBuffer.getDescriptorInfo(i);
        writes[2].setBufferInfo(camInfo);  

        vk::DescriptorBufferInfo lightInfo = lightBuffer.getDescriptorInfo(i);
        writes[3].setDstSet(m_descriptorSets[i]);
        writes[3].setDstBinding(3);
        writes[3].setDescriptorType(vk::DescriptorType::eUniformBuffer);
        writes[3].setDescriptorCount(1);
        writes[3].setBufferInfo(lightInfo);

        vk::DescriptorImageInfo opacityInfo{};
        opacityInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        opacityInfo.setImageView(m_opacityTexture.getImageView());
        opacityInfo.setSampler(m_opacityTexture.getSampler());
        writes[4].setDstSet(m_descriptorSets[i]).setDstBinding(4)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1)
            .setImageInfo(opacityInfo);

        vk::DescriptorImageInfo normalInfo{};
        normalInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        normalInfo.setImageView(m_normalTexture.getImageView());
        normalInfo.setSampler(m_normalTexture.getSampler());
        writes[5].setDstSet(m_descriptorSets[i]).setDstBinding(5)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1)
            .setImageInfo(normalInfo);

        vk::DescriptorImageInfo mrInfo{};
        mrInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        mrInfo.setImageView(m_mrTexture.getImageView());
        mrInfo.setSampler(m_mrTexture.getSampler());
        writes[6].setDstSet(m_descriptorSets[i]).setDstBinding(6)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1)
            .setImageInfo(mrInfo);

        device.updateDescriptorSets(writes, nullptr);
    }
}
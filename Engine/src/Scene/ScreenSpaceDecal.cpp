#include "Scene/ScreenSpaceDecal.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Vulkan/LightBuffer.h"
#include "Core/Log.h"

void ScreenSpaceDecal::loadTexture(vk::Device device, vk::PhysicalDevice physicalDevice,
    vk::CommandPool pool, vk::Queue queue, const std::string& path)
{
    m_texture.initialize(device, physicalDevice, pool, queue, path, true);
}

void ScreenSpaceDecal::cleanup()
{
    m_texture.cleanup();
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
        decalTexInfo.setImageView(m_texture.getImageView());
        decalTexInfo.setSampler(m_texture.getSampler());

        vk::DescriptorImageInfo depthInfo{};
        depthInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        depthInfo.setImageView(depthView);
        depthInfo.setSampler(depthSampler);

        std::array<vk::WriteDescriptorSet, 4> writes{};
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

        device.updateDescriptorSets(writes, nullptr);
    }
}
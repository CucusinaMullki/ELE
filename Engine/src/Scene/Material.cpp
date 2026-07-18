#include "Scene/Material.h"

void Material::load(vk::Device device, vk::PhysicalDevice physicalDevice,
        vk::CommandPool pool, vk::Queue queue,
        const std::string& albedoPath,
        const std::string& normalPath,
        const std::string& metallicPath,
        const std::string& roughnessPath)
{
    m_albedo.initialize(device, physicalDevice, pool, queue, albedoPath);
    m_normal.initialize(device, physicalDevice, pool, queue, normalPath);
    m_metallicRoughness.initialize(device, physicalDevice, pool, queue, metallicPath);
    m_ao.initialize(device, physicalDevice, pool, queue, roughnessPath);
}

void Material::cleanup()
{
    m_albedo.cleanup();
    m_normal.cleanup();
    m_metallicRoughness.cleanup();
    m_ao.cleanup();
}

void Material::createDescriptorSet(vk::Device device, vk::DescriptorPool pool,
    vk::DescriptorSetLayout layout,
    const std::vector<vk::DescriptorBufferInfo>& uboInfos,
    uint32_t frameCount)
{
    std::vector<vk::DescriptorSetLayout> layouts(frameCount, layout);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(pool);
    allocInfo.setSetLayouts(layouts);
    m_descriptorSets = device.allocateDescriptorSets(allocInfo);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        auto makeImageInfo = [](vk::ImageView view, vk::Sampler sampler) {
            vk::DescriptorImageInfo info{};
            info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            info.setImageView(view);
            info.setSampler(sampler);
            return info;
        };

        auto albedoInfo    = makeImageInfo(m_albedo.getImageView(), m_albedo.getSampler());
        auto normalInfo    = makeImageInfo(m_normal.getImageView(), m_normal.getSampler());
        auto mrInfo  = makeImageInfo(m_metallicRoughness.getImageView(), m_metallicRoughness.getSampler());
        auto aoInfo = makeImageInfo(m_ao.getImageView(), m_ao.getSampler());

        std::array<vk::WriteDescriptorSet, 5> writes{};

        writes[0].setDstSet(m_descriptorSets[i]);
        writes[0].setDstBinding(0);
        writes[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
        writes[0].setDescriptorCount(1);
        writes[0].setBufferInfo(uboInfos[i]);

        auto makeWrite = [&](uint32_t bind, vk::DescriptorImageInfo& info) {
            vk::WriteDescriptorSet w{};
            w.setDstSet(m_descriptorSets[i]);
            w.setDstBinding(bind);
            w.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            w.setDescriptorCount(1);
            w.setImageInfo(info);
            return w;
        };

        writes[1] = makeWrite(1, albedoInfo);
        writes[2] = makeWrite(2, normalInfo);
        writes[3] = makeWrite(3, mrInfo);
        writes[4] = makeWrite(4, aoInfo);

        device.updateDescriptorSets(writes, nullptr);
    }
}

void Material::loadFromPixels(vk::Device device, vk::PhysicalDevice physicalDevice,
    vk::CommandPool pool, vk::Queue queue,
    const unsigned char* albedoPx, int aw, int ah,
    const unsigned char* normalPx, int nw, int nh,
    const unsigned char* mrPx, int mw, int mh,
    const unsigned char* aoPx, int ow, int oh)
{
    m_albedo.initializeFromMemory(device, physicalDevice, pool, queue, albedoPx, aw, ah, true);
    m_normal.initializeFromMemory(device, physicalDevice, pool, queue, normalPx, nw, nh, false);
    m_metallicRoughness.initializeFromMemory(device, physicalDevice, pool, queue, mrPx, mw, mh, false);
    m_ao.initializeFromMemory(device, physicalDevice, pool, queue, aoPx, ow, oh, false);
}
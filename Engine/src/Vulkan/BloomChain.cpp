#include "Vulkan/BloomChain.h"
#include <stdexcept>
#include <algorithm>

void BloomChain::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D fullExtent)
{
    m_images.resize(MIP_COUNT);
    m_memories.resize(MIP_COUNT);
    m_mipViews.resize(MIP_COUNT);
    m_mipExtents.resize(MIP_COUNT);

    vk::Extent2D currentExtent = fullExtent;

    for (uint32_t level = 0; level < MIP_COUNT; level++)
    {
        currentExtent.width  = std::max(1u, currentExtent.width  / 2);
        currentExtent.height = std::max(1u, currentExtent.height / 2);
        m_mipExtents[level] = currentExtent;

        vk::ImageCreateInfo imageInfo{};
        imageInfo.setImageType(vk::ImageType::e2D);
        imageInfo.setExtent(vk::Extent3D{ currentExtent.width, currentExtent.height, 1 });
        imageInfo.setMipLevels(1);
        imageInfo.setArrayLayers(1);
        imageInfo.setFormat(m_format);
        imageInfo.setTiling(vk::ImageTiling::eOptimal);
        imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
        imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
        imageInfo.setSamples(vk::SampleCountFlagBits::e1);
        imageInfo.setSharingMode(vk::SharingMode::eExclusive);
        m_images[level] = device.createImageUnique(imageInfo);

        auto memReq = device.getImageMemoryRequirements(m_images[level].get());
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.setAllocationSize(memReq.size);
        allocInfo.setMemoryTypeIndex(findMemoryType(physicalDevice, memReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal));
        m_memories[level] = device.allocateMemoryUnique(allocInfo);
        device.bindImageMemory(m_images[level].get(), m_memories[level].get(), 0);

        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.setImage(m_images[level].get());
        viewInfo.setViewType(vk::ImageViewType::e2D);
        viewInfo.setFormat(m_format);
        viewInfo.setSubresourceRange(vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        m_mipViews[level] = device.createImageViewUnique(viewInfo);
    }

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAnisotropyEnable(false);
    samplerInfo.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    samplerInfo.setUnnormalizedCoordinates(false);
    samplerInfo.setCompareEnable(false);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    m_sampler = device.createSamplerUnique(samplerInfo);
}

void BloomChain::cleanup()
{
    m_sampler.reset();
    m_mipViews.clear();
    m_images.clear();
    m_memories.clear();
}

uint32_t BloomChain::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
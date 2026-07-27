#include "Vulkan/HistoryBuffer.h"
#include <stdexcept>

void HistoryBuffer::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent)
{
    for (int i = 0; i < 2; i++)
    {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.setImageType(vk::ImageType::e2D);
        imageInfo.setExtent(vk::Extent3D{ extent.width, extent.height, 1 });
        imageInfo.setMipLevels(1);
        imageInfo.setArrayLayers(1);
        imageInfo.setFormat(m_format);
        imageInfo.setTiling(vk::ImageTiling::eOptimal);
        imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
        imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
        imageInfo.setSamples(vk::SampleCountFlagBits::e1);
        imageInfo.setSharingMode(vk::SharingMode::eExclusive);
        m_images[i] = device.createImageUnique(imageInfo);

        auto memReq = device.getImageMemoryRequirements(m_images[i].get());
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.setAllocationSize(memReq.size);
        allocInfo.setMemoryTypeIndex(findMemoryType(physicalDevice, memReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal));
        m_memories[i] = device.allocateMemoryUnique(allocInfo);
        device.bindImageMemory(m_images[i].get(), m_memories[i].get(), 0);

        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.setImage(m_images[i].get());
        viewInfo.setViewType(vk::ImageViewType::e2D);
        viewInfo.setFormat(m_format);
        viewInfo.setSubresourceRange(vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        m_views[i] = device.createImageViewUnique(viewInfo);
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

void HistoryBuffer::cleanup()
{
    m_sampler.reset();
    for (auto& v : m_views) v.reset();
    for (auto& img : m_images) img.reset();
    for (auto& mem : m_memories) mem.reset();
}

uint32_t HistoryBuffer::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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
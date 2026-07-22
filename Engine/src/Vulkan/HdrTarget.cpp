#include "Vulkan/HdrTarget.h"
#include <stdexcept>

void HdrTarget::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent)
{
    m_extent = extent;

    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setExtent(vk::Extent3D{ extent.width, extent.height, 1 });
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setFormat(m_colorFormat);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment | 
        vk::ImageUsageFlagBits::eSampled);
    imageInfo.setSamples(vk::SampleCountFlagBits::e1);
    imageInfo.setSharingMode(vk::SharingMode::eExclusive);
    m_colorImage = device.createImageUnique(imageInfo);

    auto memReq = device.getImageMemoryRequirements(m_colorImage.get());
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memReq.size);
    allocInfo.setMemoryTypeIndex(findMemoryType(physicalDevice, memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal));
    m_colorMemory = device.allocateMemoryUnique(allocInfo);
    device.bindImageMemory(m_colorImage.get(), m_colorMemory.get(), 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(m_colorImage.get());
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setFormat(m_colorFormat);
    viewInfo.setSubresourceRange(vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    m_colorView = device.createImageViewUnique(viewInfo);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAnisotropyEnable(false);
    samplerInfo.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    samplerInfo.setCompareEnable(false);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    m_sampler = device.createSamplerUnique(samplerInfo);
}

void HdrTarget::cleanup()
{
    m_sampler.reset();
    m_colorView.reset();
    m_colorImage.reset();
    m_colorMemory.reset();    
}

uint32_t HdrTarget::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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
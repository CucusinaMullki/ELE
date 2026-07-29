#include "Vulkan/DepthBuffer.h"
#include <stdexcept>

void DepthBuffer::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent)
{
    m_format = findDepthFormat(physicalDevice);

    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setExtent(vk::Extent3D{ extent.width, extent.height, 1 });
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setFormat(m_format);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);
    imageInfo.setSamples(vk::SampleCountFlagBits::e1);
    imageInfo.setSharingMode(vk::SharingMode::eExclusive);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eNearest);
    samplerInfo.setMinFilter(vk::Filter::eNearest);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAnisotropyEnable(false);
    samplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
    samplerInfo.setUnnormalizedCoordinates(false);
    samplerInfo.setCompareEnable(false);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
    m_sampler = device.createSamplerUnique(samplerInfo);

    m_image = device.createImageUnique(imageInfo);

    auto memReq = device.getImageMemoryRequirements(m_image.get());

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memReq.size);
    allocInfo.setMemoryTypeIndex(findMemoryType(
        physicalDevice,
        memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    ));

    m_memory = device.allocateMemoryUnique(allocInfo);
    device.bindImageMemory(m_image.get(), m_memory.get(), 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(m_image.get());
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setFormat(m_format);
    viewInfo.setSubresourceRange(vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eDepth,
        0, 1, 0, 1
    });

    m_imageView = device.createImageViewUnique(viewInfo);
}

void DepthBuffer::cleanup()
{
    m_sampler.reset();
    m_imageView.reset();
    m_image.reset();
    m_memory.reset();
}

vk::Format DepthBuffer::findDepthFormat(vk::PhysicalDevice physicalDevice)
{
    std::vector<vk::Format> candidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint
    };

    for (vk::Format format : candidates)
    {
        auto props = physicalDevice.getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported depth format!");
}

uint32_t DepthBuffer::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
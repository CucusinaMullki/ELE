#include "Vulkan/CubeShadowMap.h"
#include <stdexcept>

void CubeShadowMap::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t size)
{
    m_size = size;

    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setExtent(vk::Extent3D{ size, size, 1 });
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(6);
    imageInfo.setFormat(m_format);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);
    imageInfo.setSamples(vk::SampleCountFlagBits::e1);
    imageInfo.setSharingMode(vk::SharingMode::eExclusive);
    imageInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
    m_image = device.createImageUnique(imageInfo);

    auto memReq = device.getImageMemoryRequirements(m_image.get());
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memReq.size);
    allocInfo.setMemoryTypeIndex(findMemoryType(physicalDevice, memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    m_memory = device.allocateMemoryUnique(allocInfo);
    device.bindImageMemory(m_image.get(), m_memory.get(), 0);

    for (uint32_t face = 0; face < 6; face++)
    {
        vk::ImageViewCreateInfo faceViewInfo{};
        faceViewInfo.setImage(m_image.get());
        faceViewInfo.setViewType(vk::ImageViewType::e2D);
        faceViewInfo.setFormat(m_format);
        faceViewInfo.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, face, 1 });
        m_faceViews[face] = device.createImageViewUnique(faceViewInfo);
    }

    vk::ImageViewCreateInfo cubeViewInfo{};
    cubeViewInfo.setImage(m_image.get());
    cubeViewInfo.setViewType(vk::ImageViewType::eCube);
    cubeViewInfo.setFormat(m_format);
    cubeViewInfo.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 6 });
    m_cubeView = device.createImageViewUnique(cubeViewInfo);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    samplerInfo.setAnisotropyEnable(false);
    samplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
    samplerInfo.setUnnormalizedCoordinates(false);
    samplerInfo.setCompareEnable(true);
    samplerInfo.setCompareOp(vk::CompareOp::eLessOrEqual);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    m_sampler = device.createSamplerUnique(samplerInfo);
}

void CubeShadowMap::cleanup()
{
    m_sampler.reset();
    m_cubeView.reset();
    for (auto& v : m_faceViews) v.reset();
    m_image.reset();
    m_memory.reset();
}

uint32_t CubeShadowMap::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProps = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
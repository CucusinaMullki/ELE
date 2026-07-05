#include "Vulkan/Texture.h"
#include "Core/Path.h"
#include <stb_image.h>
#include <stdexcept>
#include <cstring>

void Texture::initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool commandPool, vk::Queue graphicsQueue, const std::string& path)
{
    std::string fullPath = (Paths::assetDir() / path).string();

    int texWidth, texHeight, texChannels;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* pixels = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load texture: " + fullPath);

    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    vk::BufferCreateInfo stagingInfo{};
    stagingInfo.setSize(imageSize);
    stagingInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    stagingInfo.setSharingMode(vk::SharingMode::eExclusive);
    auto stagingBuffer = device.createBufferUnique(stagingInfo);

    auto memReq = device.getBufferMemoryRequirements(stagingBuffer.get());
    vk::MemoryAllocateInfo stagingAlloc{};
    stagingAlloc.setAllocationSize(memReq.size);
    stagingAlloc.setMemoryTypeIndex(findMemoryType(physicalDevice, memReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    auto stagingMemory = device.allocateMemoryUnique(stagingAlloc);
    device.bindBufferMemory(stagingBuffer.get(), stagingMemory.get(), 0);
    void* data = device.mapMemory(stagingMemory.get(), 0, imageSize);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    device.unmapMemory(stagingMemory.get());

    stbi_image_free(pixels);

    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setExtent(vk::Extent3D{ (uint32_t)texWidth, (uint32_t)texHeight, 1 });
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
    imageInfo.setSamples(vk::SampleCountFlagBits::e1);
    imageInfo.setSharingMode(vk::SharingMode::eExclusive);
    m_image = device.createImageUnique(imageInfo);

    auto imgMemReq = device.getImageMemoryRequirements(m_image.get());
    vk::MemoryAllocateInfo imgAlloc{};
    imgAlloc.setAllocationSize(imgMemReq.size);
    imgAlloc.setMemoryTypeIndex(findMemoryType(physicalDevice, imgMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    m_memory = device.allocateMemoryUnique(imgAlloc);
    device.bindImageMemory(m_image.get(), m_memory.get(), 0);

    transitionImageLayout(device, commandPool, graphicsQueue, m_image.get(),
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer.get(), m_image.get(), (uint32_t)texWidth, (uint32_t)texHeight);

    transitionImageLayout(device, commandPool, graphicsQueue, m_image.get(),
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(m_image.get());
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
    viewInfo.setSubresourceRange(vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    m_imageView = device.createImageViewUnique(viewInfo);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear);
    samplerInfo.setMinFilter(vk::Filter::eLinear);
    samplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
    samplerInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
    samplerInfo.setAddressModeW(vk::SamplerAddressMode::eRepeat);
    samplerInfo.setAnisotropyEnable(false);
    samplerInfo.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    samplerInfo.setUnnormalizedCoordinates(false);
    samplerInfo.setCompareEnable(false);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    m_sampler = device.createSamplerUnique(samplerInfo);
}

void Texture::cleanup()
{
    m_sampler.reset();
    m_imageView.reset();
    m_image.reset();
    m_memory.reset();
}

void Texture::transitionImageLayout(vk::Device device, vk::CommandPool pool, vk::Queue queue, 
    vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(pool);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandBufferCount(1);
    auto cmdBuffers = device.allocateCommandBuffersUnique(allocInfo);
    auto cmd = cmdBuffers[0].get();

    cmd.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::ImageMemoryBarrier barrier{};
    barrier.setOldLayout(oldLayout);
    barrier.setNewLayout(newLayout);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setImage(image);
    barrier.setSubresourceRange(vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

    vk::PipelineStageFlags srcStage, dstStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else
    {
        throw std::runtime_error("unsupported layout transition!");
    }

    cmd.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
    cmd.end();

    vk::SubmitInfo submit{};
    submit.setCommandBuffers(cmd);
    queue.submit(submit, nullptr);
    queue.waitIdle();
}

void Texture::copyBufferToImage(vk::Device device, vk::CommandPool pool, vk::Queue queue, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(pool);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandBufferCount(1);
    auto cmdBuffers = device.allocateCommandBuffersUnique(allocInfo);
    auto cmd = cmdBuffers[0].get();

    cmd.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    vk::BufferImageCopy region{};
    region.setBufferOffset(0);
    region.setBufferRowLength(0);
    region.setImageSubresource(vk::ImageSubresourceLayers{
        vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
    region.setImageOffset(vk::Offset3D{ 0, 0, 0 });
    region.setImageExtent(vk::Extent3D{ width, height, 1 });

    cmd.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    cmd.end();

    vk::SubmitInfo submit{};
    submit.setCommandBuffers(cmd);
    queue.submit(submit, nullptr);
    queue.waitIdle();
}

uint32_t Texture::findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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
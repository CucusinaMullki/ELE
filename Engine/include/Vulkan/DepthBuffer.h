#pragma once
#include <vulkan/vulkan.hpp>

class DepthBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent);
    void cleanup();

    vk::ImageView getImageView() const { return m_imageView.get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    vk::Format getFormat() const { return m_format; }

    static vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice);
    
private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::UniqueImage m_image;
    vk::UniqueDeviceMemory m_memory;
    vk::UniqueImageView m_imageView;
    vk::UniqueSampler m_sampler;
    vk::Format m_format;
};
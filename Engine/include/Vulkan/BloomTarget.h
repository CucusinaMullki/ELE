#pragma once
#include <vulkan/vulkan.hpp>

class BloomTarget
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent);
    void cleanup();

    vk::ImageView getColorView() const { return m_colorView.get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    vk::Format getColorFormat() const { return m_colorFormat; }
    vk::Extent2D getExtent() const { return m_extent;}

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::UniqueImage m_colorImage;
    vk::UniqueDeviceMemory m_colorMemory;
    vk::UniqueImageView m_colorView;
    vk::UniqueSampler m_sampler;

    vk::Format m_colorFormat = vk::Format::eR16G16B16A16Sfloat;
    vk::Extent2D m_extent;
};
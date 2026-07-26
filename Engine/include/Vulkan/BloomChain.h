#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>

class BloomChain
{
public:
    static const uint32_t MIP_COUNT = 6;

    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D fullExtent);
    void cleanup();

    vk::ImageView getMipView(uint32_t level) const { return m_mipViews[level].get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    vk::Format getFormat() const { return m_format; }
    vk::Extent2D getMipExtent(uint32_t level) const { return m_mipExtents[level]; }
    uint32_t getMipCount() const { return MIP_COUNT; }

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    std::vector<vk::UniqueImage>        m_images;
    std::vector<vk::UniqueDeviceMemory>  m_memories;
    std::vector<vk::UniqueImageView>    m_mipViews;
    std::vector<vk::Extent2D>           m_mipExtents;
    vk::UniqueSampler m_sampler;

    vk::Format m_format = vk::Format::eR16G16B16A16Sfloat;
};
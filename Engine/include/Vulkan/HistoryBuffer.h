#pragma once
#include <vulkan/vulkan.hpp>
#include <array>

class HistoryBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Extent2D extent);
    void cleanup();

    vk::ImageView getView(uint32_t index) const { return m_views[index].get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    vk::Format getFormat() const { return m_format; }

private:
    uint32_t findMemoryType(vk::PhysicalDevice physiclaDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    std::array<vk::UniqueImage, 2> m_images;
    std::array<vk::UniqueDeviceMemory, 2> m_memories;
    std::array<vk::UniqueImageView, 2> m_views;
    vk::UniqueSampler m_sampler;

    vk::Format m_format = vk::Format::eR16G16B16A16Sfloat;
};
#pragma once
#include <vulkan/vulkan.hpp>
#include <array>

class CubeShadowMap
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t size);
    void cleanup();

    vk::ImageView getFaceView(uint32_t face) const { return m_faceViews[face].get(); }
    
    vk::ImageView getCubeView() const { return m_cubeView.get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    vk::Format getFormat() const { return m_format; }
    uint32_t getSize() const { return m_size; }
    
private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::UniqueImage m_image;
    vk::UniqueDeviceMemory m_memory;
    std::array<vk::UniqueImageView, 6> m_faceViews;
    vk::UniqueImageView m_cubeView;
    vk::UniqueSampler m_sampler;

    vk::Format m_format = vk::Format::eD32Sfloat;
    uint32_t m_size = 1024;
};
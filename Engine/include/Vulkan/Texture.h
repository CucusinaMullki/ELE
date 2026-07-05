#pragma once
#include <vulkan/vulkan.hpp>
#include <string>

class Texture
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool commandPool, vk::Queue graphicsQueue, const std::string& path);
    void cleanup();

    vk::ImageView getImageView() const { return m_imageView.get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    
private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void transitionImageLayout(vk::Device device, vk::CommandPool pool, vk::Queue queue, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyBufferToImage(vk::Device device, vk::CommandPool pool, vk::Queue queue, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

    vk::UniqueImage        m_image;
    vk::UniqueDeviceMemory m_memory;
    vk::UniqueImageView    m_imageView;
    vk::UniqueSampler      m_sampler;    
};
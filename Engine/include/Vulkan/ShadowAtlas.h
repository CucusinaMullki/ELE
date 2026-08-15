#pragma once
#include <vulkan/vulkan.hpp>

class ShadowAtlas
{
public:
    static constexpr uint32_t TILE_SIZE = 512;
    static constexpr uint32_t TILES_X = 6;
    static constexpr uint32_t TILES_Y = 3;

    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice);
    void cleanup();

    vk::ImageView getImageView() const { return m_imageView.get(); }
    vk::Sampler getSampler() const { return m_sampler.get(); }
    vk::Format getFormat() const { return m_format; }

    uint32_t getWidth() const { return TILE_SIZE * TILES_X; }
    uint32_t getHeight() const { return TILE_SIZE * TILES_Y; }

    vk::Rect2D getTileRect(uint32_t lightSlot, uint32_t face) const
    {
        return vk::Rect2D{
            { (int32_t)(face * TILE_SIZE), (int32_t)(lightSlot * TILE_SIZE) },
            { TILE_SIZE, TILE_SIZE }
        };
    }

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFiler, vk::MemoryPropertyFlags properties);

    vk::UniqueImage m_image;
    vk::UniqueDeviceMemory m_memory;
    vk::UniqueImageView m_imageView;
    vk::UniqueSampler m_sampler;
    vk::Format m_format = vk::Format::eD32Sfloat;
};
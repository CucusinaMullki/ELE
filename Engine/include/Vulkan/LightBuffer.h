#pragma once
#include "Scene/Light.h"
#include <vulkan/vulkan.hpp>
#include <vector>

static const int MAX_LIGHTS = 16;

struct LightData
{
    Light lights[MAX_LIGHTS];
    glm::ivec4 count;
};

class LightBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t frameCount);
    void update(uint32_t frameIndex, const LightData& data);
    void cleanup();

    vk::DescriptorBufferInfo getDescriptorInfo(uint32_t frameIndex) const;

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    std::vector<vk::UniqueBuffer> m_buffers;
    std::vector<vk::UniqueDeviceMemory> m_memories;
    std::vector<void*> m_mapped;
};
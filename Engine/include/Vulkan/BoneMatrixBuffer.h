#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <array>
#include <vector>

static constexpr int MAX_BONES = 100;

struct BoneMatrixData
{
    std::array<glm::mat4, MAX_BONES> bones;
};

class BoneMatrixBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t frameCount);
    void update(uint32_t frameIndex, const BoneMatrixData& data);
    void cleanup();
    vk::DescriptorBufferInfo getDescriptorInfo(uint32_t frameIndex) const;

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    std::vector<vk::UniqueBuffer> m_buffers;
    std::vector<vk::UniqueDeviceMemory> m_memories;
    std::vector<void*> m_mapped;
};
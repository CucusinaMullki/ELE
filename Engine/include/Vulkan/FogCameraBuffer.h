#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>

struct FogCameraData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invViewProj;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 camPos;
};

class FogCameraBuffer
{
public:
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t frameCount);
    void update(uint32_t frameIndex, const FogCameraData& data);
    void cleanup();
    vk::DescriptorBufferInfo getDescriptorInfo(uint32_t frameIndex) const;

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilte, vk::MemoryPropertyFlags propertoes);
    std::vector<vk::UniqueBuffer> m_buffers;
    std::vector<vk::UniqueDeviceMemory> m_memories;
    std::vector<void*> m_mapped;
};
#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

//Main datas for geometry
struct MVP 
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    glm::vec4 lightPos;
    glm::vec4 lightColor;
};

class UniformBuffer
{
public:
    //Functions
    void initialize(vk::Device device, vk::PhysicalDevice physicalDevice, uint32_t frameCount);
    void update(uint32_t frameIndex, const MVP& mvp);
    void cleanup();

    //Getters
    vk::Buffer getBuffer(uint32_t frameIndex) const { return m_buffers[frameIndex].get(); }
    vk::DescriptorBufferInfo getDescriptorInfo(uint32_t frameIndex) const;

private:
    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    std::vector<vk::UniqueBuffer> m_buffers;
    std::vector<vk::UniqueDeviceMemory> m_memories;
    std::vector<void*> m_mapped;
};
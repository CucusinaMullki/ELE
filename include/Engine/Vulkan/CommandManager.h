#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

class CommandManager
{
public:
    CommandManager() = default;
    ~CommandManager() { cleanup(); }

    void intialize(vk::Device device, uint32_t queueFramilyIndex, uint32_t imageCount);
    void cleanup();

    vk::CommandBuffer getCommandBuffer(uint32_t index) const { return m_commandBuffers[index].get(); }

private:
    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
};
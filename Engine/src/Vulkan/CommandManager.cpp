#include "Vulkan/CommandManager.h"

void CommandManager::intialize(vk::Device device, uint32_t queueFramilyIndex, uint32_t imageCount)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    poolInfo.setQueueFamilyIndex(queueFramilyIndex);

    m_commandPool = device.createCommandPoolUnique(poolInfo);

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(m_commandPool.get());
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandBufferCount(imageCount);
    
    m_commandBuffers = device.allocateCommandBuffersUnique(allocInfo);
}

void CommandManager::cleanup()
{
    m_commandBuffers.clear();
    m_commandPool.reset();
}
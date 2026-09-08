#include "Vulkan/CommandManager.h"

void CommandManager::initialize(ASH::Device* device, uint32_t imageCount)
{
    m_commandBuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
        m_commandBuffers[i] = device->createCommandBuffer();
}

void CommandManager::cleanup()
{
    m_commandBuffers.clear();
}
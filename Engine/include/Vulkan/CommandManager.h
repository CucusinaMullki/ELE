#pragma once
#include "ASH/ASH.h"
#include <vector>
#include <memory>

class CommandManager
{
public:
    CommandManager() = default;
    ~CommandManager() { cleanup(); }

    void initialize(ASH::Device* device, uint32_t imageCount);
    void cleanup();

    ASH::CommandBuffer* getCommandBuffer(uint32_t index) const { return m_commandBuffers[index].get(); }

private:
    std::vector<std::unique_ptr<ASH::CommandBuffer>> m_commandBuffers;
};
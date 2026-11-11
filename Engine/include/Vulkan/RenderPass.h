#pragma once
#include "ASH/ASH.h"
#include <memory>

class RenderPass
{
public:
    RenderPass() = default;
    ~RenderPass() { cleanup(); }

    void initialize(ASH::Device* device, ASH::Format swapchainFormat, ASH::Format depthFormat);
    void cleanup();

    ASH::RenderPass* getRenderPass() const { return m_renderPass.get(); }

private:
    std::unique_ptr<ASH::RenderPass> m_renderPass;
};

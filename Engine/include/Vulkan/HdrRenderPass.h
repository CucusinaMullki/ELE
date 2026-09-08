#pragma once
#include "ASH/ASH.h"
#include <memory>

class HdrRenderPass
{
public:
    void initialize(ASH::Device* device, ASH::Format colorFormat, ASH::Format velocityFormat, ASH::Format depthFormat);
    void cleanup();

    ASH::RenderPass* getRenderPass() const { return m_renderPass.get(); }

private:
    std::unique_ptr<ASH::RenderPass> m_renderPass;
};
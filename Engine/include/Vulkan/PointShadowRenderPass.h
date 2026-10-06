#pragma once
#include "ASH/ASH.h"
#include <memory>

class PointShadowRenderPass
{
public:
    void initialize(ASH::Device* device, ASH::Format depthFormat, bool loadExisting = false);
    void cleanup();

    ASH::RenderPass* getRenderPass() const { return m_renderPass.get(); }

private:
    std::unique_ptr<ASH::RenderPass> m_renderPass;
};
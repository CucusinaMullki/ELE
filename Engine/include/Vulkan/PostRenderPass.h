#pragma once
#include "ASH/ASH.h"
#include <memory>

class PostRenderPass
{
public:
    void initialize(ASH::Device* device, ASH::Format colorFormat, ASH::LoadOp loadOp);
    void cleanup();

    ASH::RenderPass* getRenderPass() const { return m_renderPass.get(); }

private:
    std::unique_ptr<ASH::RenderPass> m_renderPass;
};
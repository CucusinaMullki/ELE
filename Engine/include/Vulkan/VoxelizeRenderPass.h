#pragma once
#include "ASH/ASH.h"
#include <memory>

class VoxelizeRenderPass
{
public:
    void initialize(ASH::Device* device);
    void cleanup();

    ASH::RenderPass* getRenderPass() const { return m_renderPass.get(); }

private:
    std::unique_ptr<ASH::RenderPass> m_renderPass;
};

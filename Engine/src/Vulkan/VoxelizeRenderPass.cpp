#include "Vulkan/VoxelizeRenderPass.h"

void VoxelizeRenderPass::initialize(ASH::Device* device)
{
    ASH::RenderPassDesc desc{};
    m_renderPass = device->createRenderPass(desc);
}

void VoxelizeRenderPass::cleanup()
{
    m_renderPass.reset();
}
#pragma once
#include "Vulkan/SwapChain.h"
#include "ASH/ASH.h"
#include <vector>
#include <memory>

class Framebuffers
{
public:
    Framebuffers() = default;
    ~Framebuffers() { cleanup(); }

    void initialize(ASH::Device* device, const SwapChain& swapChain, ASH::RenderPass* renderPass, ASH::Texture* depthTexture);
    void cleanup();

    const std::vector<std::unique_ptr<ASH::Framebuffer>>& getFramebuffers() const { return m_framebuffers; }

private:
    std::vector<std::unique_ptr<ASH::Framebuffer>> m_framebuffers;
};
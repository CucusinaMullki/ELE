#pragma once

#include "Vulkan/SwapChain.h"
#include "vulkan/vulkan.hpp"
#include <vector>

class Framebuffers
{
public:
    Framebuffers() = default;
    ~Framebuffers() { cleanup(); }

    void initialize(vk::Device device, const SwapChain& swapChain, vk::RenderPass renderPass, vk::ImageView depthImageView);
    void cleanup();

    const std::vector<vk::UniqueFramebuffer>& getFramebuffers() const { return m_framebuffers; }

private:
    std::vector<vk::UniqueFramebuffer> m_framebuffers;
};
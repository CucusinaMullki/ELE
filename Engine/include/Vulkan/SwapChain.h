#pragma once
#include "ASH/ASH.h"
#include "Vulkan/VulkanContext.h"
#include <memory>

class SwapChain
{
public:
    void initialize(VulkanContext& context, GLFWwindow* window);
    void recreate(VulkanContext& context, GLFWwindow* window);
    void cleanup();

    bool acquireNextImage(uint32_t& outImageIndex, ASH::Semaphore* signalSemaphore)
    {
        return m_swapChain->acquireNextImage(outImageIndex, signalSemaphore);
    }

    void present(uint32_t imageIndex, ASH::Semaphore* waitSemaphore)
    {
        m_swapChain->present(imageIndex, waitSemaphore);
    }

    ASH::Texture* getImage(uint32_t index) const { return m_swapChain->getImage(index); }
    uint32_t getImageCount() const { return m_swapChain->getImageCount(); }
    ASH::Extent2D getExtent() const { return m_swapChain->getExtent(); }
    ASH::Format getImageFormat() const { return m_format; }

private:
    std::unique_ptr<ASH::SwapChain> m_swapChain;
    ASH::Format m_format = ASH::Format::B8G8R8A8_UNorm;
};

#include "Vulkan/SwapChain.h"

void SwapChain::initialize(VulkanContext& context, GLFWwindow* window)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    ASH::SwapChainDesc desc{};
    desc.extent = { (uint32_t)width, (uint32_t)height };
    desc.format = m_format;
    desc.imageCount = 3;
    desc.presentMode = ASH::PresentMode::Mailbox;

    m_swapChain = context.getDevice()->createSwapChain(desc);
}

void SwapChain::recreate(VulkanContext& context, GLFWwindow* window)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    m_swapChain->resize({ (uint32_t)width, (uint32_t)height });
}

void SwapChain::cleanup()
{
    m_swapChain.reset();
}
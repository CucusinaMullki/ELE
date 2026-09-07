#include "Vulkan/VulkanContext.h"
#include <vulkan/VulkanDevice.h>
#include <stdexcept>

void VulkanContext::initialize(GLFWwindow* window)
{
#ifdef NDEBUG
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif

    m_device = ASH::createDevice(ASH::Backend::Vulkan, enableValidation);

    auto* vulkanDevice = static_cast<ASH::vulkan::VulkanDevice*>(m_device.get());
    VkResult result = glfwCreateWindowSurface(vulkanDevice->getInstance(), window, nullptr, &m_surface);
    if (result != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface via GLFW");

    m_device->attachSurface(reinterpret_cast<void*>(m_surface));
}

void VulkanContext::shutdown()
{
    if (m_surface != VK_NULL_HANDLE && m_device)
    {
        auto* vulkanDevice = static_cast<ASH::vulkan::VulkanDevice*>(m_device.get());
        vkDestroySurfaceKHR(vulkanDevice->getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    m_device.reset();
}
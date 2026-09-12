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

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    m_device = ASH::createDevice(ASH::Backend::Vulkan, enableValidation, requiredExtensions);

    auto* vulkanDevice = static_cast<ASH::vulkan::VulkanDevice*>(m_device.get());
    VkResult result = glfwCreateWindowSurface(vulkanDevice->getInstance(), window, nullptr, &m_surface);
    if (result != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface via GLFW");

    m_device->attachSurface(reinterpret_cast<void*>(m_surface));
}

void VulkanContext::shutdown()
{
    m_surface = VK_NULL_HANDLE;
    m_device.reset();
}
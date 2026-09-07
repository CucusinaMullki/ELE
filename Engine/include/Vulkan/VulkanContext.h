#pragma once
#define GLFW_INCLUDE_VULKAN
#include "ASH/ASH.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>

class VulkanContext
{
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    ASH::Device* getDevice() const { return m_device.get(); }
private:
    std::unique_ptr<ASH::Device> m_device;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};
#pragma once

#include "vulkan/vulkan.hpp"
#include "optional"
#include "GLFW/glfw3.h"
#include "vector"
#include "iostream"
#include "set"

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else 
    const bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const
    {
        return graphicFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext
{
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    //Getters
    vk::Device  getDevice() const { return m_device.get(); }
    vk::Queue getPresentQueue() const { return m_presentQueue; }
    vk::Queue getGraphicQueue() const { return m_graphicQueue; }
    vk::SurfaceKHR getSurface() const { return m_surface.get(); }
    vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    QueueFamilyIndices GetQueueFamilyIndices() const { return m_queueIndices; }

private:
    //Values
    vk::UniqueInstance m_instance;
    vk::UniqueDevice m_device;
    vk::PhysicalDevice physicalDevice;
    vk::Queue m_presentQueue;
    vk::Queue m_graphicQueue;
    vk::UniqueSurfaceKHR m_surface;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    PFN_vkDestroyDebugUtilsMessengerEXT m_destroyDebugMessengerFunc = nullptr;

    QueueFamilyIndices m_queueIndices;

    //Functions
    void createInstance(); 
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice(); 
    void createLogicalDevice();

    bool isDeviceSuitable(vk::PhysicalDevice device);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
    bool checkValidationLayerSupport();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};
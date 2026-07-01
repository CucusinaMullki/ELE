#include "Vulkan/VulkanContext.h"
#include "iostream"
#include "stdexcept"

void VulkanContext::initialize(GLFWwindow* window)
{
    try
    {
        createInstance();
        setupDebugMessenger();
        createSurface(window);     
        pickPhysicalDevice();
        createLogicalDevice();

        std::cout << "VulkanContext created without problems!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Critical error with Vulkan intialize: " << e.what() << std::endl;
        throw;
    }
}

void VulkanContext::shutdown()
{
    if (m_debugMessenger != VK_NULL_HANDLE && m_destroyDebugMessengerFunc)
    {
        m_destroyDebugMessengerFunc(static_cast<VkInstance>(m_instance.get()), m_debugMessenger, nullptr);
    }

    if (m_device)
    {
        m_device->waitIdle();
    }

    m_device.reset();
    m_surface.reset();
    m_instance.reset();

    std::cout << "VulkanContext successfully cleared!" << std::endl;
}

void VulkanContext::createInstance()
{
    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = "ELE";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); 

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    m_instance = vk::createInstanceUnique(createInfo);
}

void VulkanContext::createSurface(GLFWwindow* window)
{
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(m_instance.get()), window, nullptr, &rawSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    vk::SurfaceKHR surface(rawSurface);
    m_surface = vk::UniqueSurfaceKHR(surface, m_instance.get());
}

void VulkanContext::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    m_queueIndices = indices;

    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.queueFamilyIndex = indices.graphicFamily.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo createInfo{};
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    m_device = physicalDevice.createDeviceUnique(createInfo);

    m_graphicQueue = m_device->getQueue(indices.graphicFamily.value(), 0);
    m_presentQueue = m_device->getQueue(indices.presentFamily.value(), 0);
}

void VulkanContext::pickPhysicalDevice()
{
    auto devices = m_instance->enumeratePhysicalDevices();

    if (devices.empty())
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for (const auto& candidate : devices)
    {
        if (isDeviceSuitable(candidate))
        {
            physicalDevice = candidate;
            break;
        }
    }

    if (!physicalDevice)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool VulkanContext::isDeviceSuitable(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    auto props = device.getProperties();
    std::cout << "Checking device: " << props.deviceName << 
        " graphic=" << indices.graphicFamily.has_value() << 
        " present=" << indices.presentFamily.has_value() << std::endl;
    
    return indices.isComplete();
}

void VulkanContext::setupDebugMessenger()
{
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.messageSeverity = 
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = 
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    auto createFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        m_instance->getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        
    if (!createFunc)
    {
        throw std::runtime_error("failed to load vkCreateDebugUtilsMessengerEXT!");
    }

    VkDebugUtilsMessengerCreateInfoEXT rawCreateInfo = static_cast<VkDebugUtilsMessengerCreateInfoEXT>(createInfo);

    if (createFunc(static_cast<VkInstance>(m_instance.get()), &rawCreateInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }

    m_destroyDebugMessengerFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        m_instance->getProcAddr("vkDestroyDebugUtilsMessengerEXT"));    
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

bool VulkanContext::checkValidationLayerSupport()
{
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }
    return true;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;

    auto queueFamilies = device.getQueueFamilyProperties();

    for (uint32_t i = 0; i < queueFamilies.size(); i++)
    {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicFamily = i;
        }

        if (device.getSurfaceSupportKHR(i, m_surface.get()))
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
    }

    return indices;
}

bool VulkanContext::checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
    auto availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}
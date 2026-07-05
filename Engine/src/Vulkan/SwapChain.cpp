#include "Vulkan/SwapChain.h"

void SwapChain::initialize(const VulkanContext& context, GLFWwindow* window)
{
    auto physicalDevice = context.getPhysicalDevice();
    auto surface = context.getSurface();
    auto device = context.getDevice();

    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    auto surfaceFormat = chooseSurfaceFormat(formats);
    auto presentMode = choosePresentMode(presentModes);
    m_extent = chooseExtent(capabilities, window);
    m_imageFormat = surfaceFormat.format;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.setSurface(surface);
    createInfo.setMinImageCount(imageCount);
    createInfo.setImageFormat(surfaceFormat.format);
    createInfo.setImageColorSpace(surfaceFormat.colorSpace);
    createInfo.setImageExtent(m_extent);
    createInfo.setImageArrayLayers(1);
    createInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    auto queueIndices = context.GetQueueFamilyIndices();

    if (!queueIndices.isComplete())
    {
        throw std::runtime_error("Try to initialize SwapChain without family Queue!");
    }

    uint32_t graphicsFamily = queueIndices.graphicFamily.value();
    uint32_t presentFamily = queueIndices.presentFamily.value();
    std::array<uint32_t, 2> queueFamilyIndices = { graphicsFamily, presentFamily }; 

    if (graphicsFamily != presentFamily)
    {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent).setQueueFamilyIndices(queueFamilyIndices);
    }
    else
    {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    createInfo.setPreTransform(capabilities.currentTransform);
    createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    createInfo.setPresentMode(presentMode);
    createInfo.setClipped(true);

    if (m_swapChain)
    {
        createInfo.setOldSwapchain(m_swapChain.get());
    }
    else
    {
        createInfo.setOldSwapchain(nullptr);
    }

    m_swapChain = device.createSwapchainKHRUnique(createInfo);

    m_images = device.getSwapchainImagesKHR(m_swapChain.get());

    m_imageViews.clear();
    m_imageViews.reserve(m_images.size());

    for (const auto& image : m_images)
    {
        vk::ImageViewCreateInfo viewCreateInfo{};
        viewCreateInfo.setImage(image);
        viewCreateInfo.setViewType(vk::ImageViewType::e2D);
        viewCreateInfo.setFormat(m_imageFormat);
        viewCreateInfo.setComponents(vk::ComponentMapping{
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        });
        viewCreateInfo.setSubresourceRange(vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1
        });

        m_imageViews.push_back(device.createImageViewUnique(viewCreateInfo));
    }
}

void SwapChain::recreate(const VulkanContext& context, GLFWwindow* window)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    context.getDevice().waitIdle();

    cleanup();

    initialize(context, window);
}

void SwapChain::cleanup()
{
    m_imageViews.clear();
    m_images.clear();
    m_swapChain.reset();
}

vk::SurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return availableFormat;
            }
    }
    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::choosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapChain::chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}
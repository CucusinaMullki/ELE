#pragma once

#include "vulkan/vulkan.hpp"
#include <vector>
#include "Vulkan/VulkanContext.h"
#include <algorithm>
#include <limits>

class SwapChain
{
public:
    //Functions
    void initialize(const VulkanContext& context, GLFWwindow* window);
    void recreate(const VulkanContext& context, GLFWwindow* window);
    void cleanup(); 

    //Getters
    vk::SwapchainKHR getSwapChain() const { return m_swapChain.get(); }
    vk::Extent2D getExtent() const { return m_extent; }
    const std::vector<vk::UniqueImageView>& getImageViews() const { return m_imageViews; }
    vk::Format getImageFormat() const { return m_imageFormat; }

private:
    //Values
    vk::UniqueSwapchainKHR m_swapChain;
    std::vector<vk::Image> m_images;
    std::vector<vk::UniqueImageView> m_imageViews;
    vk::Format m_imageFormat;
    vk::Extent2D m_extent;

    //Functions
    vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes); 
    vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window); 
};

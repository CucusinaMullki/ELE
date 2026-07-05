#include "Vulkan/Renderer.h"
#include <iostream>

int main()
{
    glfwInitVulkanLoader(vkGetInstanceProcAddr); 
    glfwInit();

    if (!glfwVulkanSupported())
    {
        std::cerr << "Vulkan NOT supported by GLFW!" << std::endl;
        return -1;
    }

    uint32_t count;
    const char** ext = glfwGetRequiredInstanceExtensions(&count);
    std::cout << "GLFW requires " << count << " extensions:" << std::endl;
    for (uint32_t i = 0; i < count; i++)
        std::cout << "  " << ext[i] << std::endl;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "ELE Engine", nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Failed to create window!" << std::endl;
        return -1;
    }
    std::cout << "Window created OK" << std::endl;

    Renderer renderer;
    glfwSetWindowUserPointer(window, &renderer);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int, int) {
        auto* r = static_cast<Renderer*>(glfwGetWindowUserPointer(win));
        r->onFramebufferResize();
    });

    renderer.run(window);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
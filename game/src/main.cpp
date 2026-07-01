#include "Vulkan/Renderer.h"

int main()
{
    //Wayland support in progress
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "game", nullptr, nullptr);

    Renderer renderer;
    renderer.run(window);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
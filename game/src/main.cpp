#include "Vulkan/Renderer.h"

int main()
{
    //Wayland support in progress

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "game", nullptr, nullptr);

    Renderer renderer;
    renderer.run(window);
    renderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
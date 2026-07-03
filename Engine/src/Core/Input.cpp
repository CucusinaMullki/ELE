#include "Core/Input.h"

void Input::initialize(GLFWwindow* window)
{
    m_window = window;
    setCursorLocked(true);
}

int Input::toGLFWKey(Key key) const
{
    switch (key)
    {
        case Key::W:         return GLFW_KEY_W;
        case Key::A:         return GLFW_KEY_A;
        case Key::S:         return GLFW_KEY_S;
        case Key::D:         return GLFW_KEY_D;
        case Key::Space:     return GLFW_KEY_SPACE;
        case Key::LeftShift: return GLFW_KEY_LEFT_SHIFT;
        case Key::Escape:    return GLFW_KEY_ESCAPE;
        default:             return GLFW_KEY_UNKNOWN;
    }    
}

bool Input::isKeyPressed(Key key) const
{
    int glfwKey = toGLFWKey(key);
    if (glfwKey == GLFW_KEY_UNKNOWN)
        return false;
    
    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
}

void Input::update()
{
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);

    if (m_firstMouse)
    {
        m_lastMouseX = x;
        m_lastMouseY = y;
        m_firstMouse = false;
    }

    m_mouseDeltaX = static_cast<float>(x - m_lastMouseX);
    m_mouseDeltaY = static_cast<float>(m_lastMouseY - y);

    m_lastMouseX = x;
    m_lastMouseY = y;
}

void Input::setCursorLocked(bool locked)
{
    glfwSetInputMode(m_window, GLFW_CURSOR,
        locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}
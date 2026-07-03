#pragma once
#include <GLFW/glfw3.h>
#include <unordered_map>

enum class Key
{
    W, A, S, D,
    Space, LeftShift,
    Escape,
    Unknown
};

enum class MouseButton
{
    Left, Right, Middle,
    Unknow
};

class Input
{
public:
    void initialize(GLFWwindow* window);
    void update();

    bool isKeyPressed(Key key) const;

    float getMouseDeltaX() const { return m_mouseDeltaX; }
    float getMouseDeltaY() const { return m_mouseDeltaY; }
    
    void setCursorLocked(bool locked);

private:
    int toGLFWKey(Key key) const;

    GLFWwindow* m_window = nullptr;

    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    float m_mouseDeltaY = 0.0f;
    float m_mouseDeltaX = 0.0f;
    bool m_firstMouse = true;
};
#pragma once
#include <GLFW/glfw3.h>
#include <unordered_map>
#include "Core/IGamepad.h"
#include <string>
#include <memory>
#include <vector>

enum class Key
{
    W, A, S, D, E, Q,
    Space, LeftShift, LeftControl,
    Escape, Tab, Enter,
    Num1, Num2, Num3, Num4, Num5,
    F1, F2, F3, F4, F5,
    GraveAccent,
    Unknown
};

enum class MouseButton
{
    Left, Right, Middle,
    Unknown
};

struct InputBinding
{
    enum class Type { Keyboard, Mouse, Gamepad } type = Type::Keyboard;
    Key key = Key::Unknown;
    MouseButton mouseButton = MouseButton::Unknown;
    GamepadButton gamepadButton = GamepadButton::Unknown;

    static InputBinding fromKey(Key k) { return { Type::Keyboard, k, MouseButton::Unknown }; }
    static InputBinding fromMouse(MouseButton b) { return { Type::Mouse, Key::Unknown, b }; }
    static InputBinding fromGamepad(GamepadButton b) { return { Type::Gamepad, Key::Unknown, MouseButton::Unknown, b }; }
};

class Input
{
public:
    void initialize(GLFWwindow* window);
    void update();

    bool isKeyPressed(Key key) const;
    bool isKeyJustPressed(Key key) const;
    bool isKeyJustReleased(Key key) const;

    bool isMouseButtonPressed(MouseButton button) const;
    bool isMouseButtonJustPressed(MouseButton button) const;

    float getMouseDeltaX() const { return m_mouseDeltaX; }
    float getMouseDeltaY() const { return m_mouseDeltaY; }
    float getScrollDelta() const { return m_scrollDelta; }

    void setCursorLocked(bool locked);
    bool isCursorLocked() const { return m_cursorLocked; }

    void bindAction(const std::string& action, InputBinding binding);
    bool isActionPressed(const std::string& action) const;
    bool isActionJustPressed(const std::string& action) const;

    IGamepad& gamepad() { return *m_gamepad; }
    const IGamepad& gamepad() const { return *m_gamepad; }

private:
    int toGLFWKey(Key key) const;
    int toGLFWMouseButton(MouseButton button) const;
    bool rawKeyPressed(Key key) const;
    bool rawMousePressed(MouseButton button) const;

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    GLFWwindow* m_window = nullptr;

    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    float m_mouseDeltaX = 0.0f;
    float m_mouseDeltaY = 0.0f;
    bool m_firstMouse = true;
    bool m_cursorLocked = true;

    static inline float s_scrollAccumulator = 0.0f;
    float m_scrollDelta = 0.0f;

    std::unordered_map<int, bool> m_currKeyState;
    std::unordered_map<int, bool> m_prevKeyState;
    std::unordered_map<int, bool> m_currMouseState;
    std::unordered_map<int, bool> m_prevMouseState;

    std::unordered_map<std::string, std::vector<InputBinding>> m_actionBindings;

    std::unique_ptr<IGamepad> m_gamepad;
};
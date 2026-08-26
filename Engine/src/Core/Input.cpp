#include "Core/Input.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include "Core/GLFWGamepad.h"
#include <fstream>

void Input::initialize(GLFWwindow* window)
{
    m_window = window;
    setCursorLocked(true);
    glfwSetScrollCallback(window, scrollCallback);

    std::string dbPath = (Paths::assetDir() / "assets" / "gamecontrollerdb.txt").string();
    LOG("looking for gamecontrollerdb at: " << dbPath);
    std::ifstream file(dbPath);
    if (file.is_open())
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string mappings = buffer.str();
        int result = glfwUpdateGamepadMappings(mappings.c_str());
        LOG("gamecontrollerdb loaded, glfwUpdateGamepadMappings result: " << result);
    }
    else
    {
        LOG("gamecontrollerdb.txt not found — using GLFW built-in mappings only");
    }

    m_gamepad = std::make_unique<GLFWGamepad>(GLFW_JOYSTICK_1);
}

void Input::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    s_scrollAccumulator += (float)yoffset;
}

int Input::toGLFWKey(Key key) const
{
    switch (key)
    {
        case Key::W:            return GLFW_KEY_W;
        case Key::A:            return GLFW_KEY_A;
        case Key::S:            return GLFW_KEY_S;
        case Key::D:            return GLFW_KEY_D;
        case Key::Q:            return GLFW_KEY_Q;
        case Key::E:            return GLFW_KEY_E;
        case Key::Space:        return GLFW_KEY_SPACE;
        case Key::LeftShift:    return GLFW_KEY_LEFT_SHIFT;
        case Key::LeftControl:  return GLFW_KEY_LEFT_CONTROL;
        case Key::Escape:       return GLFW_KEY_ESCAPE;
        case Key::Tab:          return GLFW_KEY_TAB;
        case Key::Enter:        return GLFW_KEY_ENTER;
        case Key::Num1:         return GLFW_KEY_1;
        case Key::Num2:         return GLFW_KEY_2;
        case Key::Num3:         return GLFW_KEY_3;
        case Key::Num4:         return GLFW_KEY_4;
        case Key::Num5:         return GLFW_KEY_5;
        case Key::F1:           return GLFW_KEY_F1;
        case Key::F2:           return GLFW_KEY_F2;
        case Key::F3:           return GLFW_KEY_F3;
        case Key::F4:           return GLFW_KEY_F4;
        case Key::F5:           return GLFW_KEY_F5;
        case Key::GraveAccent:   return GLFW_KEY_GRAVE_ACCENT;
        default:                return GLFW_KEY_UNKNOWN;
    }
}

int Input::toGLFWMouseButton(MouseButton button) const
{
    switch (button)
    {
        case MouseButton::Left:   return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::Right:  return GLFW_MOUSE_BUTTON_RIGHT;
        case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
        default:                  return -1;
    }
}

bool Input::rawKeyPressed(Key key) const
{
    int glfwKey = toGLFWKey(key);
    if (glfwKey == GLFW_KEY_UNKNOWN) return false;
    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
}

bool Input::rawMousePressed(MouseButton button) const
{
    int glfwButton = toGLFWMouseButton(button);
    if (glfwButton < 0) return false;
    return glfwGetMouseButton(m_window, glfwButton) == GLFW_PRESS;
}

bool Input::isKeyPressed(Key key) const { return rawKeyPressed(key); }

bool Input::isKeyJustPressed(Key key) const
{
    int k = toGLFWKey(key);
    auto curr = m_currKeyState.find(k);
    auto prev = m_prevKeyState.find(k);
    bool currPressed = (curr != m_currKeyState.end()) && curr->second;
    bool prevPressed = (prev != m_prevKeyState.end()) && prev->second;
    return currPressed && !prevPressed;
}

bool Input::isKeyJustReleased(Key key) const
{
    int k = toGLFWKey(key);
    auto curr = m_currKeyState.find(k);
    auto prev = m_prevKeyState.find(k);
    bool currPressed = (curr != m_currKeyState.end()) && curr->second;
    bool prevPressed = (prev != m_prevKeyState.end()) && prev->second;
    return !currPressed && prevPressed;
}

bool Input::isMouseButtonPressed(MouseButton button) const { return rawMousePressed(button); }

bool Input::isMouseButtonJustPressed(MouseButton button) const
{
    int b = toGLFWMouseButton(button);
    auto curr = m_currMouseState.find(b);
    auto prev = m_prevMouseState.find(b);
    bool currPressed = (curr != m_currMouseState.end()) && curr->second;
    bool prevPressed = (prev != m_prevMouseState.end()) && prev->second;
    return currPressed && !prevPressed;
}

void Input::update()
{
    m_prevKeyState = m_currKeyState;
    m_prevMouseState = m_currMouseState;

    for (int k = 0; k < (int)Key::Unknown; k++)
    {
        int glfwKey = toGLFWKey((Key)k);
        if (glfwKey != GLFW_KEY_UNKNOWN)
            m_currKeyState[glfwKey] = (glfwGetKey(m_window, glfwKey) == GLFW_PRESS);
    }
    for (int b = 0; b < (int)MouseButton::Unknown; b++)
    {
        int glfwButton = toGLFWMouseButton((MouseButton)b);
        if (glfwButton >= 0)
            m_currMouseState[glfwButton] = (glfwGetMouseButton(m_window, glfwButton) == GLFW_PRESS);
    }

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

    m_scrollDelta = s_scrollAccumulator;
    s_scrollAccumulator = 0.0f;

    m_gamepad->update();
}

void Input::setCursorLocked(bool locked)
{
    m_cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR,
        locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Input::bindAction(const std::string& action, InputBinding binding)
{
    m_actionBindings[action].push_back(binding);
}

bool Input::isActionPressed(const std::string& action) const
{
    auto it = m_actionBindings.find(action);
    if (it == m_actionBindings.end()) return false;

    for (const auto& binding : it->second)
    {
        if (binding.type == InputBinding::Type::Keyboard && rawKeyPressed(binding.key))
            return true;
        if (binding.type == InputBinding::Type::Mouse && rawMousePressed(binding.mouseButton))
            return true;
        if (binding.type == InputBinding::Type::Gamepad && m_gamepad->isButtonPressed(binding.gamepadButton))
            return true;
    }
    return false;
}

bool Input::isActionJustPressed(const std::string& action) const
{
    auto it = m_actionBindings.find(action);
    if (it == m_actionBindings.end()) return false;

    for (const auto& binding : it->second)
    {
        if (binding.type == InputBinding::Type::Keyboard && isKeyJustPressed(binding.key))
            return true;
        if (binding.type == InputBinding::Type::Mouse && isMouseButtonJustPressed(binding.mouseButton))
            return true;
        if (binding.type == InputBinding::Type::Gamepad && m_gamepad->isButtonJustPressed(binding.gamepadButton))
            return true;
    }
    return false;
}
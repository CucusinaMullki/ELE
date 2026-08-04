#include "Core/GLFWGamepad.h"
#include <cstring>
#include <cmath>

void GLFWGamepad::update()
{
    m_prevState = m_currState;
    
    if (glfwJoystickIsGamepad(m_jid) && glfwGetGamepadState(m_jid, &m_currState))
        m_hasState = true;
    else
        m_hasState = false;
}

bool GLFWGamepad::isConnected() const
{
    return glfwJoystickPresent(m_jid) && glfwJoystickIsGamepad(m_jid);
}

int GLFWGamepad::toGLFWButton(GamepadButton button) const
{
    switch (button)
    {
        case GamepadButton::A:           return GLFW_GAMEPAD_BUTTON_A;
        case GamepadButton::B:           return GLFW_GAMEPAD_BUTTON_B;
        case GamepadButton::X:           return GLFW_GAMEPAD_BUTTON_X;
        case GamepadButton::Y:           return GLFW_GAMEPAD_BUTTON_Y;
        case GamepadButton::LeftBumper:  return GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
        case GamepadButton::RightBumper: return GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
        case GamepadButton::Back:        return GLFW_GAMEPAD_BUTTON_BACK;
        case GamepadButton::Start:       return GLFW_GAMEPAD_BUTTON_START;
        case GamepadButton::Guide:       return GLFW_GAMEPAD_BUTTON_GUIDE;
        case GamepadButton::LeftThumb:   return GLFW_GAMEPAD_BUTTON_LEFT_THUMB;
        case GamepadButton::RightThumb:  return GLFW_GAMEPAD_BUTTON_RIGHT_THUMB;
        case GamepadButton::DPadUp:      return GLFW_GAMEPAD_BUTTON_DPAD_UP;
        case GamepadButton::DPadRight:   return GLFW_GAMEPAD_BUTTON_DPAD_RIGHT;
        case GamepadButton::DPadDown:    return GLFW_GAMEPAD_BUTTON_DPAD_DOWN;
        case GamepadButton::DPadLeft:    return GLFW_GAMEPAD_BUTTON_DPAD_LEFT;
        default:                        return -1;
    }
}

int GLFWGamepad::toGLFWAxis(GamepadAxis axis) const
{
    switch (axis)
    {
        case GamepadAxis::LeftX:        return GLFW_GAMEPAD_AXIS_LEFT_X;
        case GamepadAxis::LeftY:        return GLFW_GAMEPAD_AXIS_LEFT_Y;
        case GamepadAxis::RightX:       return GLFW_GAMEPAD_AXIS_RIGHT_X;
        case GamepadAxis::RightY:       return GLFW_GAMEPAD_AXIS_RIGHT_Y;
        case GamepadAxis::LeftTrigger:  return GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
        case GamepadAxis::RightTrigger: return GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
        default:                        return -1;
    }
}

bool GLFWGamepad::isButtonPressed(GamepadButton button) const
{
    if (!m_hasState) return false;
    int b = toGLFWButton(button);
    if (b < 0) return false;
    return m_currState.buttons[b] == GLFW_PRESS;
}

bool GLFWGamepad::isButtonJustPressed(GamepadButton button) const
{
    if (!m_hasState) return false;
    int b = toGLFWButton(button);
    if (b < 0) return false;
    return m_currState.buttons[b] == GLFW_PRESS && m_prevState.buttons[b] != GLFW_PRESS;
}

float GLFWGamepad::getAxis(GamepadAxis axis) const
{
    if (!m_hasState) return 0.0f;
    int a = toGLFWAxis(axis);
    if (a < 0) return 0.0f;

    float value = m_currState.axes[a];

    if (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger)
        value = (value + 1.0f) * 0.5f;

    if (axis == GamepadAxis::LeftX || axis == GamepadAxis::LeftY || 
        axis == GamepadAxis::RightX || axis == GamepadAxis::RightY)
    {
        if (std::abs(value) < DEADZONE) value = 0.0f;   
    }

    return value;
}
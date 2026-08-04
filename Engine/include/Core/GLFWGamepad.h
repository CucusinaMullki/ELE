#pragma once
#include "Core/IGamepad.h"
#include <GLFW/glfw3.h>
#include <array>

class GLFWGamepad : public IGamepad
{
public:
    explicit GLFWGamepad(int jid) : m_jid(jid) {}

    void update() override;

    bool isConnected() const override;
    bool isButtonPressed(GamepadButton button) const override;
    bool isButtonJustPressed(GamepadButton button) const override;
    float getAxis(GamepadAxis axis) const override;

private:
    int toGLFWButton(GamepadButton button) const;
    int toGLFWAxis(GamepadAxis axis) const;

    int m_jid;
    GLFWgamepadstate m_currState{};
    GLFWgamepadstate m_prevState{};
    bool m_hasState = false;

    static constexpr float DEADZONE = 0.15f;
};
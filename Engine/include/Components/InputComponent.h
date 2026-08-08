#pragma once
#include "Scene/IComponent.h"
#include "Core/Input.h"
#include <glm/glm.hpp>

class InputComponent : public IComponent
{
public:
    explicit InputComponent(Input& input) : m_input(input) {}

    glm::vec2 getMoveAxis() const
    {
        glm::vec2 move(0.0f);
        if (m_input.isKeyPressed(Key::W)) move.y += 1.0f;
        if (m_input.isKeyPressed(Key::S)) move.y -= 1.0f;
        if (m_input.isKeyPressed(Key::D)) move.x += 1.0f;
        if (m_input.isKeyPressed(Key::A)) move.x -= 1.0f;

        move.x += m_input.gamepad().getAxis(GamepadAxis::LeftX);
        move.y += -m_input.gamepad().getAxis(GamepadAxis::LeftY);

        if (glm::length(move) > 1.0f)
            move = glm::normalize(move);

        return move;
    }

    glm::vec2 getLookDelta() const
    {
        float gamepadYaw = m_input.gamepad().getAxis(GamepadAxis::RightX) * m_gamepadLookSpeed;
        float gamepadPitch = -m_input.gamepad().getAxis(GamepadAxis::RightY) * m_gamepadLookSpeed;

        return glm::vec2(m_input.getMouseDeltaX() + gamepadYaw, m_input.getMouseDeltaY() + gamepadPitch);
    }

    bool isJumpJustPressed() const { return m_input.isActionJustPressed("Up"); }
    bool isFirePressed() const { return m_input.isActionPressed("Fire"); }
    bool isFireJustPressed() const { return m_input.isActionJustPressed("Fire"); }

    void setGamepadLookSpeed(float speed) { m_gamepadLookSpeed = speed; }

private:
    Input& m_input;
    float m_gamepadLookSpeed = 150.0f;
};
#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Components/InputComponent.h"
#include "Components/CharacterComponent.h"
#include "Components/CameraComponent.h"
#include <glm/glm.hpp>

class PlayerComponent : public IComponent
{
public:
    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;

        m_input = owner.getComponent<InputComponent>();
        m_camera = owner.getComponent<CameraComponent>();
        m_character = owner.getComponent<CharacterComponent>();
    }

    void update(float deltaTime) override
    {
        if (!m_input) return;

        glm::vec2 look = m_input->getLookDelta();
        m_owner->transform.rotation.y += look.x * m_lookSensitivity;

        m_pitch = glm::clamp(m_pitch + look.y * m_lookSensitivity, -89.0f, 89.0f);
        if (m_camera)
            m_camera->setPitch(m_pitch);

        if (m_character)
        {
            glm::vec2 moveAxis = m_input->getMoveAxis();
            float yawRad = glm::radians(m_owner->transform.rotation.y);

            glm::vec3 forward(sin(yawRad), 0.0f, -cos(yawRad));
            glm::vec3 right(cos(yawRad), 0.0f, sin(yawRad));

            glm::vec3 moveDir = forward * moveAxis.y + right * moveAxis.x;
            if (glm::length(moveDir) > 0.0001f)
                moveDir = glm::normalize(moveDir);

            m_character->setMoveInput(moveDir * m_moveSpeed);

            if (m_input->isJumpJustPressed())
                m_character->jump(m_jumpSpeed);
        }

        if (m_camera)
            m_camera->applyToCamera();
    }

    void setLookSensitivity(float s) { m_lookSensitivity = s; }
    void setMoveSpeed(float s) { m_moveSpeed = s; }
    void setJumpSpeed(float s) { m_jumpSpeed = s; }

private:
    GameObject* m_owner = nullptr;
    InputComponent* m_input = nullptr;
    CameraComponent* m_camera = nullptr;
    CharacterComponent* m_character = nullptr;

    float m_pitch = 0.0f;
    float m_lookSensitivity = 0.1f;
    float m_moveSpeed = 2.5f;
    float m_jumpSpeed = 5.0f;
};
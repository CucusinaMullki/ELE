#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Components/InputComponent.h"
#include "Components/CharacterComponent.h"
#include "HeadBobComponent.h"
#include "Components/CameraComponent.h"
#include "InteractableComponent.h"
#include <glm/glm.hpp>

class PlayerComponent : public IComponent
{
public:
    explicit PlayerComponent(PhysicsSystem& physics) : m_physics(physics) {}

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;

        m_input = owner.getComponent<InputComponent>();
        m_camera = owner.getComponent<CameraComponent>();
        m_character = owner.getComponent<CharacterComponent>();
        m_headBob = owner.getComponent<HeadBobComponent>();
    }

    void update(float deltaTime) override
    {
        if (!m_input) return;

        if (m_input->isFireJustPressed())
        {
            glm::vec3 origin = m_camera->getCamera().getPosition();
            glm::vec3 dir = m_camera->getCamera().getFront();

            RaycastHit hit = m_physics.raycast(origin, dir, 3.0f);
            if (hit.hit)
            {
                GameObject* target = m_physics.getOwner(hit.bodyId);
                if (target)
                {
                    LOG("Looked at object: " << target->name << ", distance=" << hit.distance);
                }
                else
                {
                    LOG("Hit unnamed body at distance=" << hit.distance);
                }
            }
            else
            {
                LOG("Raycast: nothing in range");
            }
        }

        if (m_character)
            m_character->syncTransform(deltaTime);

        if (!m_lookInitialized)
        {
            m_targetYaw   = m_owner->transform.rotation.y;
            m_targetPitch = m_pitch;
            m_lookInitialized = true;
        }

        glm::vec2 look = m_input->getLookDelta();
        m_targetYaw  += look.x * m_lookSensitivity;
        m_targetPitch = glm::clamp(m_targetPitch + look.y * m_lookSensitivity, -89.0f, 89.0f);

        if (m_lookSmoothing > 0.0f)
        {
            const float dt = glm::min(deltaTime, 0.05f);
            const float t  = 1.0f - std::exp(-m_lookSmoothing * dt);
            m_owner->transform.rotation.y += (m_targetYaw   - m_owner->transform.rotation.y) * t;
            m_pitch                       += (m_targetPitch - m_pitch) * t;
        }
        else
        {
            m_owner->transform.rotation.y = m_targetYaw;
            m_pitch = m_targetPitch;
        }

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

        if (m_input->isInteractJustPressed())
        {
            LOG("Interact pressed!");

            glm::vec3 origin = m_camera->getCamera().getPosition();
            glm::vec3 dir = m_camera->getCamera().getFront();
            RaycastHit hit = m_physics.raycast(origin, dir, m_interactRange);

            LOG("Raycast hit: " << hit.hit << ", distance: " << hit.distance);

            if (hit.hit)
            {
                GameObject* target = m_physics.getOwner(hit.bodyId);
                LOG("target found: " << (target != nullptr) << ", name: '" << (target ? target->name : "null") << "'");

                if (target)
                {
                    auto* interactable = target->getComponent<InteractableComponent>();
                    LOG("interactable found: " << (interactable != nullptr));

                    if (interactable && interactable->onInteract)
                        interactable->onInteract();
                }
            }
            else
            {
                LOG("Interact: nothing in range");
            }
        }

        if (m_headBob)
            m_headBob->applyBob(deltaTime);

        if (m_camera)
            m_camera->applyToCamera();
    }

    void setLookSmoothing(float s) { m_lookSmoothing = s; }
    void setLookSensitivity(float s) { m_lookSensitivity = s; }
    void setMoveSpeed(float s) { m_moveSpeed = s; }
    void setJumpSpeed(float s) { m_jumpSpeed = s; }

private:
    GameObject* m_owner = nullptr;
    InputComponent* m_input = nullptr;
    CameraComponent* m_camera = nullptr;
    CharacterComponent* m_character = nullptr;
    HeadBobComponent* m_headBob = nullptr;
    PhysicsSystem& m_physics;

    float m_interactRange = 3.0f;

    float m_pitch = 0.0f;
    float m_lookSensitivity = 0.1f;
    float m_moveSpeed = 2.5f;
    float m_jumpSpeed = 5.0f;
    float m_targetYaw = 0.0f;
    float m_targetPitch = 0.0f;
    bool  m_lookInitialized = false;
    float m_lookSmoothing = 20.0f;
};

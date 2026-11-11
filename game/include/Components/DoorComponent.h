#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Components/RigidBodyComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class DoorComponent : public IComponent
{
public:
    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        m_rigidBody = owner.getComponent<RigidBodyComponent>();

        m_closedRotationY = owner.transform.rotation.y;
        m_openRotationY = m_closedRotationY + m_openAngle;
    }

    void update(float deltaTime) override
    {
        float target = m_isOpen ? m_openRotationY : m_closedRotationY;
        float current = m_owner->transform.rotation.y;

        float t = 1.0f - exp(-m_openSpeed * deltaTime);
        m_owner->transform.rotation.y = glm::mix(current, target, t);

        if (m_rigidBody)
        {
            glm::quat rot = glm::quat(glm::radians(m_owner->transform.rotation));
            m_rigidBody->setTransform(m_owner->transform.position, rot);
        }
    }

    void toggle() { m_isOpen = !m_isOpen; }

    void setOpenAngle(float degrees) { m_openAngle = degrees; m_openRotationY = m_closedRotationY + m_openAngle; }
    void setOpenSpeed(float speed) { m_openSpeed = speed; }
    bool isOpen() const { return m_isOpen; }

private:
    GameObject* m_owner = nullptr;
    RigidBodyComponent* m_rigidBody = nullptr;

    bool m_isOpen = false;
    float m_closedRotationY = 0.0f;
    float m_openRotationY = 0.0f;
    float m_openAngle = 90.0f;
    float m_openSpeed = 4.0f;
};

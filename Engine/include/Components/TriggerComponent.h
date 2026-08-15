#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Physics/PhysicsSystem.h"
#include <glm/glm.hpp>
#include <functional>

class TriggerComponent : public IComponent
{
public:
    TriggerComponent(PhysicsSystem& physics, glm::vec3 halfExtent)
        : m_physics(physics), m_halfExtent(halfExtent)
    {}

    ~TriggerComponent()
    {
        m_physics.getContactListener().unregisterTrigger(m_bodyId);
    }

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        m_bodyId = m_physics.createTriggerBox(owner.transform.position, m_halfExtent);
        m_physics.registerBody(m_bodyId, &owner);
        m_physics.getContactListener().registerTrigger(m_bodyId, this);
    }

    void fireEnter(JPH::BodyID other) { if (onTriggerEnter) onTriggerEnter(other); }
    void fireExit(JPH::BodyID other) { if (onTriggerExit) onTriggerExit(other); }

    std::function<void(JPH::BodyID)> onTriggerEnter;
    std::function<void(JPH::BodyID)> onTriggerExit;

private:
    PhysicsSystem& m_physics;
    GameObject* m_owner = nullptr;
    JPH::BodyID m_bodyId;
    glm::vec3 m_halfExtent;
};
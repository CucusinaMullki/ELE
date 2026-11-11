#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Physics/PhysicsSystem.h"
#include <glm/glm.hpp>
#include <functional>
#include <mutex>
#include <unordered_set>

class TriggerComponent : public IComponent
{
public:
    TriggerComponent(PhysicsSystem& physics, glm::vec3 halfExtent)
        : m_physics(physics), m_halfExtent(halfExtent)
    {}

    ~TriggerComponent() override { destroyBody(); }

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        createBody();
    }

    const char* typeName() const override { return "Trigger"; }

    void visit(PropertyVisitor& v) override
    {
        v.field("halfExtent", m_halfExtent);
    }

    void onPropertiesChanged() override
    {
        destroyBody();
        createBody();
    }

    void fireEnter(JPH::BodyID other)
    {
        { std::lock_guard lock(m_mutex); m_inside.insert(other.GetIndexAndSequenceNumber()); }
        if (onTriggerEnter) onTriggerEnter(other);
    }
    void fireExit(JPH::BodyID other)
    {
        { std::lock_guard lock(m_mutex); m_inside.erase(other.GetIndexAndSequenceNumber()); }
        if (onTriggerExit) onTriggerExit(other);
    }

    bool contains(JPH::BodyID body) const
    {
        std::lock_guard lock(m_mutex);
        return m_inside.count(body.GetIndexAndSequenceNumber()) != 0;
    }
    bool empty() const { std::lock_guard lock(m_mutex); return m_inside.empty(); }

    JPH::BodyID getBodyId() const { return m_bodyId; }

    std::function<void(JPH::BodyID)> onTriggerEnter;
    std::function<void(JPH::BodyID)> onTriggerExit;

private:
    void createBody()
    {
        m_bodyId = m_physics.createTriggerBox(m_owner->transform.position, m_halfExtent);
        m_physics.registerBody(m_bodyId, m_owner);
        m_physics.getContactListener().registerTrigger(m_bodyId, this);
    }

    void destroyBody()
    {
        if (m_bodyId.IsInvalid()) return;
        m_physics.getContactListener().unregisterTrigger(m_bodyId);
        m_physics.destroyBody(m_bodyId);
        m_bodyId = JPH::BodyID();
        std::lock_guard lock(m_mutex);
        m_inside.clear();
    }

    PhysicsSystem& m_physics;
    GameObject* m_owner = nullptr;
    JPH::BodyID m_bodyId;
    glm::vec3 m_halfExtent;

    mutable std::mutex m_mutex;
    std::unordered_set<uint32_t> m_inside;
};

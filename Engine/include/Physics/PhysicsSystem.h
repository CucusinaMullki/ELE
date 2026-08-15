#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include "Core/PhysicsLayers.h"
#include <glm/gtc/quaternion.hpp> 
#include <glm/glm.hpp>
#include <memory>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <vector>
#include <unordered_map>
#include <functional>

struct RaycastHit
{
    bool hit = false;
    JPH::BodyID bodyId;
    glm::vec3 point{0.0f};
    float distance = 0.0f;
};

class TriggerComponent;
class GameObject;

class TriggerContactListener : public JPH::ContactListener
{
public:
    void registerTrigger(JPH::BodyID id, TriggerComponent* trigger) { m_triggers[id] = trigger; }
    void unregisterTrigger(JPH::BodyID id) { m_triggers.erase(id); }

    void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
        const JPH::ContactManifold&, JPH::ContactSettings&) override
    {
        dispatch(body1.GetID(), body2.GetID(), true);
    }

    void notifyEnter(JPH::BodyID trigger, JPH::BodyID other) { dispatch(trigger, other, true); }
    void notifyExit(JPH::BodyID trigger, JPH::BodyID other) { dispatch(trigger, other, false); }

    void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override
    {
        dispatch(subShapePair.GetBody1ID(), subShapePair.GetBody2ID(), false);
    }

private:
    void dispatch(JPH::BodyID a, JPH::BodyID b, bool entered);

    std::unordered_map<JPH::BodyID, TriggerComponent*> m_triggers;
};

class PhysicsSystem
{
public:
    void initialize();
    void update(float fixedDeltaTime);
    void cleanup();

    JPH::BodyInterface& bodyInterface() { return m_physicsSystem->GetBodyInterface(); }

    JPH::BodyID createStaticBox(glm::vec3 position, glm::vec3 halfExtent);
    JPH::BodyID createDynamicBox(glm::vec3 position, glm::vec3 halfExtent, float mass);

    JPH::BodyID createStaticSphere(glm::vec3 position, float radius);
    JPH::BodyID createDynamicSphere(glm::vec3 position, float radius, float mass);

    JPH::BodyID createStaticMesh(const std::vector<glm::vec3>& positions, const std::vector<uint32_t>& indices, glm::vec3 worldPosition);
    JPH::BodyID createDynamicConvexHull(const std::vector<glm::vec3>& positions, glm::vec3 worldPosition, float mass);

    JPH::BodyID createTriggerBox(glm::vec3 position, glm::vec3 halfExtent);

    JPH::Ref<JPH::CharacterVirtual> createCharacterVirtual(glm::vec3 position, float radius, float height, float mass = 80.0f);
    void updateCharacter(JPH::CharacterVirtual* character, float deltaTime);

    glm::vec3 getBodyPosition(JPH::BodyID id) const;
    glm::quat getBodyRotation(JPH::BodyID id) const;
    TriggerContactListener& getContactListener() { return m_contactListener; }

    void registerBody(JPH::BodyID id, GameObject* owner) { m_bodyOwners[id.GetIndexAndSequenceNumber()] = owner; }
    GameObject* getOwner(JPH::BodyID id) const
    {
        auto it = m_bodyOwners.find(id.GetIndexAndSequenceNumber());
        return (it != m_bodyOwners.end()) ? it->second : nullptr;
    }

    RaycastHit raycast(glm::vec3 origin, glm::vec3 direction, float maxDistance);

    std::vector<JPH::BodyID> queryOverlappingBodies(glm::vec3 position, float radius);
    bool isSensor(JPH::BodyID id) const;

private:
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

    BPLayerInterfaceImpl m_broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl m_objectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl m_objectLayerPairFilter;

    TriggerContactListener m_contactListener;

    std::unordered_map<JPH::uint32, GameObject*> m_bodyOwners;

    static constexpr JPH::uint MAX_BODIES = 1024;
    static constexpr JPH::uint NUM_BODY_MUTEXES = 0;
    static constexpr JPH::uint MAX_BODY_PAIRS = 1024;
    static constexpr JPH::uint MAX_CONTACT_CONSTRAINTS = 1024;
};
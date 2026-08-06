#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include "Core/PhysicsLayers.h"
#include <glm/gtc/quaternion.hpp> 
#include <glm/glm.hpp>
#include <memory>

class PhysicsSystem
{
public:
    void initialize();
    void update(float fixedDeltaTime);
    void cleanup();

    JPH::BodyInterface& bodyInterface() { return m_physicsSystem->GetBodyInterface(); }

    JPH::BodyID createStaticBox(glm::vec3 position, glm::vec3 halfExtent);

    JPH::BodyID createDynamicBox(glm::vec3 position, glm::vec3 halfExtent, float mass);

    JPH::BodyID createStaticMesh(const std::vector<glm::vec3>& positions, const std::vector<uint32_t>& indices, glm::vec3 worldPosition);
    JPH::BodyID createDynamicConvexHull(const std::vector<glm::vec3>& positions, glm::vec3 worldPosition, float mass);

    glm::vec3 getBodyPosition(JPH::BodyID id) const;
    glm::quat getBodyRotation(JPH::BodyID id) const;

private:
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

    BPLayerInterfaceImpl m_broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl m_objectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl m_objectLayerPairFilter;

    static constexpr JPH::uint MAX_BODIES = 1024;
    static constexpr JPH::uint NUM_BODY_MUTEXES = 0;
    static constexpr JPH::uint MAX_BODY_PAIRS = 1024;
    static constexpr JPH::uint MAX_CONTACT_CONSTRAINTS = 1024;
};
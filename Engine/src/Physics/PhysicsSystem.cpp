#include "Physics/PhysicsSystem.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include "Components/TriggerComponent.h"
#include <thread>

std::vector<JPH::BodyID> PhysicsSystem::queryOverlappingBodies(glm::vec3 position, float radius)
{
    std::vector<JPH::BodyID> result;

    JPH::SphereShape sphere(radius);
    JPH::RMat44 transform = JPH::RMat44::sTranslation(JPH::RVec3(position.x, position.y, position.z));

    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    JPH::CollideShapeSettings settings;

    m_physicsSystem->GetNarrowPhaseQuery().CollideShape(&sphere, JPH::Vec3::sReplicate(1.0f),
        transform, settings, JPH::RVec3::sZero(), collector);

    for (const auto& hit : collector.mHits)
        result.push_back(hit.mBodyID2);

    return result;
}

bool PhysicsSystem::isSensor(JPH::BodyID id) const
{
    JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), id);
    if (!lock.Succeeded()) return false;
    return lock.GetBody().IsSensor();
}

void TriggerContactListener::dispatch(JPH::BodyID a, JPH::BodyID b, bool entered)
{
    auto itA = m_triggers.find(a);
    if (itA != m_triggers.end())
        entered ? itA->second->fireEnter(b) : itA->second->fireExit(b);

    auto itB = m_triggers.find(b);
    if (itB != m_triggers.end())
        entered ? itB->second->fireEnter(a) : itB->second->fireExit(a);
}

JPH::BodyID PhysicsSystem::createStaticMesh(const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices, glm::vec3 worldPosition)
{
    JPH::VertexList vertexList;
    vertexList.reserve(positions.size());
    for (const auto& p : positions)
        vertexList.push_back(JPH::Float3(p.x, p.y, p.z));

    JPH::IndexedTriangleList triangleList;
    triangleList.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
        triangleList.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2]));

    JPH::MeshShapeSettings shapeSettings(vertexList, triangleList);
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(worldPosition.x, worldPosition.y, worldPosition.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

RaycastHit PhysicsSystem::raycast(glm::vec3 origin, glm::vec3 direction, float maxDistance)
{
    RaycastHit result{};

    JPH::RVec3 rOrigin(origin.x, origin.y, origin.z);
    JPH::Vec3 rDirection(direction.x * maxDistance, direction.y * maxDistance, direction.z * maxDistance);

    JPH::RRayCast ray{ rOrigin, rDirection };
    JPH::RayCastResult hit;

    bool found = m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hit);

    if (found)
    {
        result.hit = true;
        result.bodyId = hit.mBodyID;
        result.distance = hit.mFraction * maxDistance;

        JPH::RVec3 hitPoint = rOrigin + rDirection * hit.mFraction;
        result.point = glm::vec3(hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ());

        JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded())
        {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPoint);
            result.normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
    }

    return result;
}

JPH::Ref<JPH::CharacterVirtual> PhysicsSystem::createCharacterVirtual(
    glm::vec3 position, float radius, float height, float mass)
{
    JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(0.5f * height, radius);
    JPH::RefConst<JPH::Shape> shape = new JPH::RotatedTranslatedShape(
        JPH::Vec3(0, 0.5f * height + radius, 0), JPH::Quat::sIdentity(), capsule);

    JPH::CharacterVirtualSettings settings;
    settings.mShape = shape;
    settings.mMass = mass;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);

    auto character = new JPH::CharacterVirtual(&settings,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(), 0, m_physicsSystem.get());

    return character;
}

void PhysicsSystem::updateCharacter(JPH::CharacterVirtual* character, float deltaTime)
{
    character->Update(deltaTime, m_physicsSystem->GetGravity(),
        m_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        m_physicsSystem->GetDefaultLayerFilter(Layers::MOVING),
        {}, {}, *m_tempAllocator);
}

JPH::BodyID PhysicsSystem::createStaticSphere(glm::vec3 position, float radius)
{
    JPH::SphereShapeSettings shapeSettings(radius);
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING);

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsSystem::createDynamicSphere(glm::vec3 position, float radius, float mass)
{
    JPH::SphereShapeSettings shapeSettings(radius);
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);

    settings.mMassPropertiesOverride.mMass = mass;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

JPH::BodyID PhysicsSystem::createTriggerBox(glm::vec3 position, glm::vec3 halfExtent)
{
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING);
    settings.mIsSensor = true;

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsSystem::createDynamicConvexHull(const std::vector<glm::vec3>& positions,
    glm::vec3 worldPosition, float mass)
{
    JPH::Array<JPH::Vec3> points;
    points.reserve(positions.size());
    for (const auto& p : positions)
        points.push_back(JPH::Vec3(p.x, p.y, p.z));

    JPH::ConvexHullShapeSettings shapeSettings(points);
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(worldPosition.x, worldPosition.y, worldPosition.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    settings.mMassPropertiesOverride.mMass = mass;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

void PhysicsSystem::initialize()
{
    JPH::RegisterDefaultAllocator();

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    uint32_t numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads);

    m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_physicsSystem->Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
        m_broadPhaseLayerInterface, m_objectVsBroadPhaseLayerFilter, m_objectLayerPairFilter);

    m_physicsSystem->SetContactListener(&m_contactListener);

    m_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
}

void PhysicsSystem::update(float fixedDeltaTime)
{
    const int collisionSteps = 1;
    m_physicsSystem->Update(fixedDeltaTime, collisionSteps, m_tempAllocator.get(), m_jobSystem.get());
}

JPH::BodyID PhysicsSystem::createStaticBox(glm::vec3 position, glm::vec3 halfExtent)
{
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsSystem::createDynamicBox(glm::vec3 position, glm::vec3 halfExtent, float mass)
{
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    settings.mMassPropertiesOverride.mMass = mass;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

glm::vec3 PhysicsSystem::getBodyPosition(JPH::BodyID id) const
{
    JPH::RVec3 pos = m_physicsSystem->GetBodyInterface().GetPosition(id);
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

glm::quat PhysicsSystem::getBodyRotation(JPH::BodyID id) const
{
    JPH::Quat rot = m_physicsSystem->GetBodyInterface().GetRotation(id);
    return glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
}

void PhysicsSystem::cleanup()
{
    m_physicsSystem.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsSystem::setBodyTransform(JPH::BodyID id, glm::vec3 position, glm::quat rotation)
{
    JPH::RVec3 jPos(position.x, position.y, position.z);
    JPH::Quat jRot(rotation.x, rotation.y, rotation.z, rotation.w);

    bodyInterface().SetPosition(id, jPos, JPH::EActivation::Activate);
    bodyInterface().SetRotation(id, jRot, JPH::EActivation::Activate);
}

JPH::BodyID PhysicsSystem::createKinematicConvexHull(const std::vector<glm::vec3>& positions, glm::vec3 worldPosition)
{
    JPH::Array<JPH::Vec3> points;
    points.reserve(positions.size());
    for (const auto& p : positions)
        points.push_back(JPH::Vec3(p.x, p.y, p.z));

    JPH::ConvexHullShapeSettings shapeSettings(points);
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(worldPosition.x, worldPosition.y, worldPosition.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Kinematic,
        Layers::MOVING);

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

JPH::BodyID PhysicsSystem::createKinematicBox(glm::vec3 position, glm::vec3 halfExtent)
{
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings settings(shape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Kinematic,
        Layers::MOVING);

    return bodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}
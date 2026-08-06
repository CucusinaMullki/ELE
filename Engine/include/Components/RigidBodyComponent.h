#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Physics/PhysicsSystem.h"
#include <Jolt/Physics/Body/BodyID.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "Scene/Mesh.h"

class RigidBodyComponent : public IComponent
{
public:
    enum class Shape { Box, Sphere, Mesh, ConvexHull };
    enum class Motion { Static, Dynamic };

    RigidBodyComponent(PhysicsSystem& physics, Shape shape, Motion motion,
        std::shared_ptr<Mesh> collisionMesh = nullptr,
        glm::vec3 halfExtentOrRadius = glm::vec3(0.5f), float mass = 1.0f)
        : m_physics(physics), m_shape(shape), m_motion(motion)
        , m_collisionMesh(collisionMesh), m_size(halfExtentOrRadius), m_mass(mass)
    {}

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        glm::vec3 startPos = owner.transform.position;
    
        switch (m_shape)
        {
            case Shape::Box:
                m_bodyId = (m_motion == Motion::Static)
                    ? m_physics.createStaticBox(startPos, m_size)
                    : m_physics.createDynamicBox(startPos, m_size, m_mass);
                break;
        
            case Shape::Mesh:
            {
                std::vector<glm::vec3> positions;
                for (const auto& v : m_collisionMesh->getCpuVertices())
                    positions.push_back(v.pos);
            
                m_bodyId = m_physics.createStaticMesh(positions, m_collisionMesh->getCpuIndices(), startPos);
                break;
            }
        
            case Shape::ConvexHull:
            {
                std::vector<glm::vec3> positions;
                for (const auto& v : m_collisionMesh->getCpuVertices())
                    positions.push_back(v.pos);
            
                m_bodyId = m_physics.createDynamicConvexHull(positions, startPos, m_mass);
                break;
            }
        
            default: break;
        }
    }

    void update(float deltaTime) override
    {
        if (m_motion == Motion::Dynamic)
        {
            m_owner->transform.position = m_physics.getBodyPosition(m_bodyId);

            glm::quat q = m_physics.getBodyRotation(m_bodyId);
            m_owner->transform.rotation = glm::degrees(glm::eulerAngles(q));
        }
    }

    JPH::BodyID getBodyId() const { return m_bodyId; }

private:
    PhysicsSystem& m_physics;
    GameObject* m_owner = nullptr;
    JPH::BodyID m_bodyId;

    Shape m_shape;
    Motion m_motion;
    std::shared_ptr<Mesh> m_collisionMesh;
    glm::vec3 m_size;
    float m_mass;
};
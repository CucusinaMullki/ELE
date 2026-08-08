#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Physics/PhysicsSystem.h"
#include "Core/TimeSystem.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <glm/glm.hpp>

class CharacterComponent : public IComponent
{
public:
    CharacterComponent(PhysicsSystem& physics, TimeSystem& time,
        float radius = 0.3f, float height = 1.8f, float mass = 80.0f)
        : m_physics(physics), m_time(time), m_radius(radius), m_height(height), m_mass(mass)
    {}

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        m_character = m_physics.createCharacterVirtual(owner.transform.position, m_radius, m_height, m_mass);
        m_previousPosition = m_currentPosition = owner.transform.position;
    }

    void fixedUpdate(float fixedDeltaTime) override
    {
        m_previousPosition = m_currentPosition;

        JPH::Vec3 velocity(m_moveInput.x, m_verticalVelocity, m_moveInput.z);
        m_character->SetLinearVelocity(velocity);
        m_physics.updateCharacter(m_character.GetPtr(), fixedDeltaTime);

        if (isOnGround())
            m_verticalVelocity = 0.0f;
        else
            m_verticalVelocity -= 9.81f * fixedDeltaTime;

        m_currentPosition = rawPosition();
    }

    void update(float deltaTime) override
    {
        float alpha = glm::clamp(m_time.getFixedAlpha(), 0.0f, 1.0f);
        m_owner->transform.position = glm::mix(m_previousPosition, m_currentPosition, alpha);
    }

    void setMoveInput(glm::vec3 worldSpaceHorizontal) { m_moveInput = worldSpaceHorizontal; }

    void jump(float jumpSpeed = 5.0f)
    {
        if (isOnGround())
            m_verticalVelocity = jumpSpeed;
    }

    bool isOnGround() const
    {
        return m_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
    }

    glm::vec3 getPosition() const { return m_owner->transform.position; }

private:
    glm::vec3 rawPosition() const
    {
        JPH::RVec3 pos = m_character->GetPosition();
        return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
    }

    PhysicsSystem& m_physics;
    TimeSystem& m_time;
    GameObject* m_owner = nullptr;
    JPH::Ref<JPH::CharacterVirtual> m_character;

    float m_radius, m_height, m_mass;
    glm::vec3 m_moveInput{0.0f};
    float m_verticalVelocity = 0.0f;

    glm::vec3 m_previousPosition{0.0f};
    glm::vec3 m_currentPosition{0.0f};
};
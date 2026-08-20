#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Physics/PhysicsSystem.h"
#include "Core/TimeSystem.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <glm/glm.hpp>
#include <algorithm>

class CharacterComponent : public IComponent
{
public:
    CharacterComponent(PhysicsSystem& physics, TimeSystem& time,
        float radius = 0.3f, float height = 1.8f, float mass = 80.0f)
        : m_physics(physics), m_time(time), m_radius(radius), m_height(height), m_mass(mass)
    {}

    ~CharacterComponent()
    {
        s_allCharacters.erase(std::remove(s_allCharacters.begin(), s_allCharacters.end(), this), s_allCharacters.end());
    }

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        m_character = m_physics.createCharacterVirtual(owner.transform.position, m_radius, m_height, m_mass);
        m_previousPosition = m_currentPosition = owner.transform.position;

        s_allCharacters.push_back(this);
    }

    void syncTransform(float deltaTime)
    {
        float alpha = glm::clamp(m_time.getFixedAlpha(), 0.0f, 1.0f);
        m_owner->transform.position = glm::mix(m_previousPosition, m_currentPosition, alpha);
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

        resolveCharacterOverlap();
    
        std::vector<JPH::BodyID> overlapping = m_physics.queryOverlappingBodies(rawPosition(), m_radius);

        std::vector<JPH::BodyID> currentTriggers;
        for (auto id : overlapping)
            if (m_physics.isSensor(id))
                currentTriggers.push_back(id);

        for (auto id : currentTriggers)
        {
            bool wasInside = std::find(m_previousTriggers.begin(), m_previousTriggers.end(), id) != m_previousTriggers.end();
            if (!wasInside)
                m_physics.getContactListener().notifyEnter(id, m_bodyIdPlaceholder);
        }
        for (auto id : m_previousTriggers)
        {
            bool stillInside = std::find(currentTriggers.begin(), currentTriggers.end(), id) != currentTriggers.end();
            if (!stillInside)
                m_physics.getContactListener().notifyExit(id, m_bodyIdPlaceholder);
        }

        m_previousTriggers = currentTriggers;
    }

    glm::vec3 getVelocity() const { return glm::vec3(m_moveInput.x, m_verticalVelocity, m_moveInput.z); }

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

    void resolveCharacterOverlap()
    {
        for (CharacterComponent* other : s_allCharacters)
        {
            if (other == this) continue;

            glm::vec3 toOther = other->m_currentPosition - m_currentPosition;
            toOther.y = 0.0f;

            float distance = glm::length(toOther);
            float combinedRadius = m_radius + other->m_radius;

            if (distance < combinedRadius && distance > 0.001f)
            {
                glm::vec3 pushDir = toOther / distance;
                float overlap = combinedRadius - distance;

                m_currentPosition -= pushDir * overlap * 0.5f;
                other->m_currentPosition += pushDir * overlap * 0.5f;
            }
        }
    }

    static inline std::vector<CharacterComponent*> s_allCharacters;

    std::vector<JPH::BodyID> m_previousTriggers;
    JPH::BodyID m_bodyIdPlaceholder;

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
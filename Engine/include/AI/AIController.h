#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Physics/PhysicsSystem.h"
#include "Core/Log.h"
#include "Components/CharacterComponent.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class ICommand;

class AIController : public IComponent
{
public:
    AIController(PhysicsSystem& physics) : m_physics(physics) {}

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        m_character = owner.getComponent<CharacterComponent>();
    }

    void moveTowards(glm::vec3 direction, float speed)
    {
        if (m_character)
            m_character->setMoveInput(direction * speed);
    }

    void faceDirection(glm::vec3 direction)
    {
        if (glm::length(direction) < 0.001f) return;
        float yawRad = atan2(direction.x, -direction.z);
        m_owner->transform.rotation.y = glm::degrees(yawRad);
    }

    void update(float deltaTime) override
    {
        if (m_character)
            m_character->syncTransform(deltaTime);

        static float logTimer = 0.0f;
        logTimer += deltaTime;
        if (logTimer >= 1.0f)
        {
            LOG("enemy pos: " << m_owner->transform.position.x << ","
                << m_owner->transform.position.y << "," << m_owner->transform.position.z);
            logTimer = 0.0f;
        }

        updateVision();
        updateCommands(deltaTime);
    }

    void setVisionTarget(GameObject* target) { m_visionTarget = target; }
    bool canSeeTarget() const { return m_canSeeTarget; }
    glm::vec3 getLastKnowTargetPosition() const { return m_lastKnowTargetPos; }
    glm::vec3 getOwnerPosition() const { return m_owner->transform.position; }

    void setVisionRange(float range) { m_visionRange = range;}
    void setVisionFOV(float degrees) { m_visionFOVCos = cos(glm::radians(degrees * 0.5f)); }
    void setEyeOffset(glm::vec3 offset) { m_eyeOffset = offset; }

    void pushCommand(std::unique_ptr<ICommand> command);
    void clearCommands();

private:
    void updateVision();
    void updateCommands(float deltaTime);

    PhysicsSystem& m_physics;
    GameObject* m_owner = nullptr;

    GameObject* m_visionTarget = nullptr;
    bool m_canSeeTarget = false;
    glm::vec3 m_lastKnowTargetPos{0.0f};

    float m_visionRange = 15.0f;
    float m_visionFOVCos = cos(glm::radians(60.0f));
    glm::vec3 m_eyeOffset{0.0f, 1.6f, 0.0f};

    CharacterComponent* m_character = nullptr;

    std::vector<std::unique_ptr<ICommand>> m_commandQueue;
};

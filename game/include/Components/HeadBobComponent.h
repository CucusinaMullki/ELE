#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Components/CameraComponent.h"
#include "Components/CharacterComponent.h"
#include <glm/glm.hpp>
#include <cmath>

class HeadBobComponent : public IComponent
{
public:
    HeadBobComponent(glm::vec3 baseEyeOffset)
        : m_baseEyeOffset(baseEyeOffset)
    {}

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        m_camera = owner.getComponent<CameraComponent>();
        m_character = owner.getComponent<CharacterComponent>();
    }

    void applyBob(float deltaTime)
    {
        if (!m_camera || !m_character) return;

        float speed = glm::length(glm::vec2(m_character->getVelocity().x, m_character->getVelocity().z));

        if (speed > m_moveThreshold && m_character->isOnGround())
            updateBob(deltaTime);
        else
            resetBob(deltaTime);
    }

    void setBobFrequency(float f) { m_bobFrequency = f; }
    void setAmplitudeVertical(float a) { m_bobAmplitudeV = a; }
    void setAmplitudeHorizontal(float a) { m_bobAmplitudeH = a; }
    void setStepRollAngle(float degrees) { m_stepRollAngle = degrees; }
    void setRollInterpSpeed(float s) { m_rollInterpSpeed = s; }
    void setRollResetSpeed(float s) { m_rollResetSpeed = s; }
    void setBobResetSpeed(float s) { m_bobResetSpeed = s; }

private:
    static float expInterpTo(float current, float target, float deltaTime, float speed)
    {
        if (speed <= 0.0f) return target;
        float t = 1.0f - exp(-speed * deltaTime);
        return glm::mix(current, target, t);
    }

    static glm::vec3 expInterpTo(glm::vec3 current, glm::vec3 target, float deltaTime, float speed)
    {
        return glm::vec3(
            expInterpTo(current.x, target.x, deltaTime, speed),
            expInterpTo(current.y, target.y, deltaTime, speed),
            expInterpTo(current.z, target.z, deltaTime, speed)
        );
    }

    void updateBob(float deltaTime)
    {
        m_bobTime += deltaTime * m_bobFrequency;
        float sinValue = sin(m_bobTime);

        if (m_lastSinValue <= 0.0f && sinValue > 0.0f)
        {
            m_stepSide = !m_stepSide;
            m_targetRoll = m_stepSide ? m_stepRollAngle : -m_stepRollAngle;
        }
        m_lastSinValue = sinValue;

        glm::vec3 offset = m_baseEyeOffset;
        offset.y += sinValue * m_bobAmplitudeV;
        offset.x += sin(m_bobTime * 0.5f) * m_bobAmplitudeH;

        m_camera->setEyeOffset(offset);

        m_targetRoll = expInterpTo(m_targetRoll, 0.0f, deltaTime, m_rollResetSpeed);
        m_currentRoll = expInterpTo(m_currentRoll, m_targetRoll, deltaTime, m_rollInterpSpeed);

        m_camera->getCamera().setRoll(m_currentRoll);
    }

    void resetBob(float deltaTime)
    {
        glm::vec3 currentOffset = m_camera->getEyeOffset();
        glm::vec3 target = expInterpTo(currentOffset, m_baseEyeOffset, deltaTime, m_bobResetSpeed);
        m_camera->setEyeOffset(target);

        m_currentRoll = expInterpTo(m_currentRoll, 0.0f, deltaTime, m_rollResetSpeed);
        m_targetRoll = 0.0f;
        m_camera->getCamera().setRoll(m_currentRoll);

        m_bobTime = expInterpTo(m_bobTime, 0.0f, deltaTime, m_bobResetSpeed);
        m_lastSinValue = 0.0f;
    }

    GameObject* m_owner = nullptr;
    CameraComponent* m_camera = nullptr;
    CharacterComponent* m_character = nullptr;

    glm::vec3 m_baseEyeOffset;

    float m_bobTime = 0.0f;
    float m_lastSinValue = 0.0f;
    bool m_stepSide = false;

    float m_currentRoll = 0.0f;
    float m_targetRoll = 0.0f;

    float m_moveThreshold = 0.1f;
    float m_bobFrequency = 10.0f;
    float m_bobAmplitudeV = 0.04f;
    float m_bobAmplitudeH = 0.02f;
    float m_stepRollAngle = 2.5f;
    float m_rollInterpSpeed = 6.0f;
    float m_rollResetSpeed = 15.0f;
    float m_bobResetSpeed = 10.0f;
};

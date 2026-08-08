#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Core/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class CameraComponent : public IComponent
{
public:
    enum class Mode { FirstPerson, ThirdPerson };
    
    CameraComponent(Camera& targetCamera, Mode mode = Mode::FirstPerson)
        : m_camera(targetCamera), m_mode(mode)
    {}

    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void setPitch(float degreesPitch) { m_pitch = degreesPitch; }

    void applyToCamera()
    {
        if (!m_active) return;

        glm::vec3 basePos = m_owner->transform.position;
        float yawRad = glm::radians(m_owner->transform.rotation.y);
        float pitchRad = glm::radians(m_pitch);
        
        glm::vec3 forward;
        forward.x = cos(pitchRad) * sin(yawRad);
        forward.y = sin(pitchRad);
        forward.z = -cos(pitchRad) * cos(yawRad);
        forward = glm::normalize(forward);

        glm::vec3 camPos = (m_mode == Mode::FirstPerson)
            ? basePos + m_eyeOffset
            : basePos + m_eyeOffset - forward * m_armLength;

        m_camera.setPosition(camPos);
        m_camera.lookAt(camPos + forward);
    }

    void setActive(bool active) { m_active = active; }
    bool isActive() const { return m_active; }

    void setEyeOffset(glm::vec3 offset) { m_eyeOffset = offset; }
    void setArmLength(float length) { m_armLength = length; }
    void setMode(Mode mode) { m_mode = mode; }

private:
    Camera& m_camera;
    GameObject* m_owner = nullptr;

    Mode m_mode;
    bool m_active = true;

    float m_pitch = 0.0f;

    glm::vec3 m_eyeOffset{0.0f, 1.6f, 0.0f};
    float m_armLength = 4.0f;
};
#include "Core/OrbitCameraController.h"
#include "Core/Camera.h"
#include "Core/Input.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void OrbitCameraController::onActivate(Camera& camera)
{
    glm::vec3 toCam = camera.getPosition() - m_target;
    m_distance = glm::length(toCam);
    if (m_distance < 0.01f) m_distance = m_minDistance;

    m_yaw = glm::degrees(atan2(toCam.z, toCam.x));
    m_pitch = glm::degrees(asin(glm::clamp(toCam.y / m_distance, -1.0f, 1.0f)));
}

void OrbitCameraController::update(Camera& camera, const Input& input, float deltaTime)
{
    if (input.isMouseButtonPressed(MouseButton::Right))
    {
        m_yaw += input.getMouseDeltaX() * m_rotateSpeed;
        m_pitch -= input.getMouseDeltaY() * m_rotateSpeed;
        m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
    }

    m_distance -= input.getScrollDelta() * m_zoomSpeed;
    m_distance = glm::clamp(m_distance, m_minDistance, m_maxDistance);

    float yawRad = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);

    glm::vec3 offset;
    offset.x = m_distance * cos(pitchRad) * cos(yawRad);
    offset.y = m_distance * sin(pitchRad);
    offset.z = m_distance * cos(pitchRad) * sin(yawRad);

    camera.setPosition(m_target + offset);
    camera.lookAt(m_target);
}

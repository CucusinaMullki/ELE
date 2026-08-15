#include "Core/Camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void Camera::initialize(glm::vec3 position)
{
    m_position = position;
    updateVectors();
}

void Camera::processKeyboard(glm::vec3 direction, float deltaTime)
{
    float velocity = m_moveSpeed * deltaTime;
    m_position += direction * velocity;
}

void Camera::lookAt(glm::vec3 target)
{
    m_front = glm::normalize(target - m_position);
    m_yaw = glm::degrees(atan2(m_front.z ,m_front.x));
    m_pitch = glm::degrees(asin(m_front.y));
    updateVectors();
}

void Camera::processMouse(float deltaX, float deltaY)
{
    m_yaw += deltaX * m_mouseSensitivity;
    m_pitch += deltaY * m_mouseSensitivity;

    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    glm::vec3 rolledUp = glm::rotate(m_up, glm::radians(m_roll), m_front);
    return glm::lookAt(m_position, m_position + m_front, rolledUp);
}

void Camera::updateVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_front));    
}

glm::mat4 Camera::getProjectionMatrix() const
{
    glm::mat4 proj = glm::perspective(
        glm::radians(m_fov),
        m_aspect,
        m_nearPlane,
        m_farPlane
    );
    proj[1][1] *= -1; 
    return proj;
}
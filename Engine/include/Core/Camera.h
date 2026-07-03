#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
    void initialize(glm::vec3 position);

    void processKeyboard(glm::vec3 direction, float deltaTime);
    void processMouse(float deltaX, float deltaY);

    glm::mat4 getViewMatrix() const;

    glm::vec3 getFront() const { return m_front; }
    glm::vec3 getRight() const { return m_right; }
    glm::vec3 getPosition() const { return m_position; }

private:
    void updateVectors();

    glm::vec3 m_position{0.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;

    float m_moveSpeed = 3.0f;
    float m_mouseSensitivity = 0.1f;
};
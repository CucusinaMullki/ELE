#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
    void initialize(glm::vec3 position);

    void processKeyboard(glm::vec3 direction, float deltaTime);
    void processMouse(float deltaX, float deltaY);

    void setAspect(float aspect) { m_aspect = aspect; }
    void setPosition(glm::vec3 pos) { m_position = pos; }

    void setRoll(float degreesRoll) { m_roll = degreesRoll; }
    float getRoll() const { return m_roll; }

    void lookAt(glm::vec3 target);

    glm::mat4 getViewMatrix() const;
    glm::vec3 getFront() const { return m_front; }
    glm::vec3 getRight() const { return m_right; }
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getUp() const  { return m_up; }
    glm::mat4 getProjectionMatrix() const;

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

    float m_aspect = 16.0f / 9.0f;
    float m_fov = 45.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
    float m_roll = 0.0f;
};
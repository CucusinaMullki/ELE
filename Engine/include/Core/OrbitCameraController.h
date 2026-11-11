#pragma once
#include "Core/ICameraController.h"
#include <glm/glm.hpp>

class OrbitCameraController : public ICameraController
{
public:
    void setTarget(glm::vec3 target) { m_target = target; }

    void update(Camera& camera, const Input& input, float deltaTime) override;
    void onActivate(Camera& camera) override;

private:
    glm::vec3 m_target{0.0f};
    float m_distance = 5.0f;
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;

    float m_rotateSpeed = 0.2f;
    float m_zoomSpeed = 1.0f;
    float m_minDistance = 1.0f;
    float m_maxDistance = 30.0f;
};

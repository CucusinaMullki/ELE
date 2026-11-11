#pragma once
#include "Scene/Transform.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

inline Transform decomposeTransform(const glm::mat4& m)
{
    Transform t;
    t.position = glm::vec3(m[3]);

    glm::vec3 c0(m[0]), c1(m[1]), c2(m[2]);
    t.scale = { glm::length(c0), glm::length(c1), glm::length(c2) };
    if (glm::dot(glm::cross(c0, c1), c2) < 0.0f) t.scale.x = -t.scale.x;
    c0 /= t.scale.x; c1 /= t.scale.y; c2 /= t.scale.z;

    const float pitch = std::asin(std::clamp(-c2.y, -1.0f, 1.0f));
    const float yaw = std::atan2(c2.x, c2.z);
    const glm::vec3 r0(std::cos(yaw), 0.0f, -std::sin(yaw));
    const glm::vec3 u0(std::sin(pitch) * std::sin(yaw), std::cos(pitch), std::sin(pitch) * std::cos(yaw));
    const float roll = std::atan2(glm::dot(c0, u0), glm::dot(c0, r0));

    t.rotation = glm::degrees(glm::vec3(pitch, yaw, roll));
    return t;
}

inline glm::quat eulerToQuat(const glm::vec3& deg)
{
    glm::mat4 m(1.0f);
    m = glm::rotate(m, glm::radians(deg.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(deg.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(deg.z), glm::vec3(0, 0, 1));
    return glm::normalize(glm::quat_cast(glm::mat3(m)));
}

inline glm::vec3 quatToEuler(const glm::quat& q)
{
    return decomposeTransform(glm::mat4_cast(glm::normalize(q))).rotation;
}

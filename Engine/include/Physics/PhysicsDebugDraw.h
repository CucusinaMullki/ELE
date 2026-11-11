#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace JPH { class PhysicsSystem; }

struct DebugLine
{
    glm::vec3 a, b;
    uint32_t color;
};

struct PhysicsDebugOptions
{
    bool colliders = true;
    bool triggers = true;
    bool staticBodies = true;
    bool boundingBoxes = false;
    uint32_t triggerColor = 0xFF20D0FFu;
};

bool collectPhysicsDebugLines(JPH::PhysicsSystem& system, const PhysicsDebugOptions& opt,
    glm::vec3 cameraPos, std::vector<DebugLine>& out);

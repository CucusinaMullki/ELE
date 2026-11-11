#pragma once
#include "Physics/PhysicsSystem.h"
#include <glm/glm.hpp>

struct WalkabilityResult
{
    bool walkable = false;
    glm::vec3 groundPoint{0.0f};
};

class GeometryParser
{
public:
    static WalkabilityResult isWalkable(PhysicsSystem& physics, glm::vec3 point,
        float maxSlopeDegrees = 45.0f, float charaterHeihgt = 1.8f,
        float rayStartHeight = 10.0f, float maxRayDistance = 20.0f);
};

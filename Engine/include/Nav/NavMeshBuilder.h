#pragma once
#include "Nav/NavMesh.h"
#include "Physics/PhysicsSystem.h"
#include <glm/glm.hpp>

class NavMeshBuilder
{
public:
    virtual ~NavMeshBuilder() = default;

    virtual NavMesh build(PhysicsSystem& physics, glm::vec3 volumeMin, glm::vec3 volumeMax, float cellSize) = 0;
};

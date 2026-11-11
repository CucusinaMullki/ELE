#pragma once
#include "Nav/NavMeshBuilder.h"

class SimpleNavMeshBuilder : public NavMeshBuilder
{
public:
    NavMesh build(PhysicsSystem& physics, glm::vec3 volumeMin, glm::vec3 volumeMax, float cellSize) override;

private:
    bool hasLineOfSight(PhysicsSystem& physics, glm::vec3 a, glm::vec3 b) const;
};

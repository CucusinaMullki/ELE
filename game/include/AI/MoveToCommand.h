#pragma once
#include "AI/ICommand.h"
#include "AI/AIController.h"
#include "Nav/NavMesh.h"
#include <glm/glm.hpp>
#include <vector>

class MoveToCommand : public ICommand
{
public:
    MoveToCommand(const NavMesh& navMesh, glm::vec3 targetPos, float speed = 3.0f, float arriveThreshold = 0.3f)
        : m_navMesh(navMesh), m_targetPos(targetPos), m_speed(speed), m_arriveThreshold(arriveThreshold)
    {}

    bool execute(AIController& ai, float deltaTime) override;

private:
    const NavMesh& m_navMesh;
    glm::vec3 m_targetPos;
    float m_speed;
    float m_arriveThreshold;

    std::vector<glm::vec3> m_path;
    bool m_pathComputed = false;
    size_t m_currentWaypoint = 0;
};
#pragma once
#include "AI/ICommand.h"
#include "MoveToCommand.h"
#include "Nav/NavMesh.h"
#include <vector>

class PatrolCommand : public ICommand
{
public:
    PatrolCommand(const NavMesh& navMesh, std::vector<glm::vec3> waypoints, float speed = 2.5f)
        : m_navMesh(navMesh), m_waypoints(std::move(waypoints)), m_speed(speed)
    {}

    bool execute(AIController& ai, float deltaTime) override;
    
private:
    const NavMesh& m_navMesh;
    std::vector<glm::vec3> m_waypoints;
    float m_speed;
    size_t m_currentIndex = 0;
    std::unique_ptr<MoveToCommand> m_currentMove;
};
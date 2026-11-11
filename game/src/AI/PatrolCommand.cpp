#include "AI/PatrolCommand.h"

bool PatrolCommand::execute(AIController& ai, float deltaTime)
{
    if (m_waypoints.empty()) return true;

    if (ai.canSeeTarget())
        return true;

    if (!m_currentMove)
    {
        m_currentMove = std::make_unique<MoveToCommand>(m_navMesh, m_waypoints[m_currentIndex], m_speed);
    }

    bool finished = m_currentMove->execute(ai, deltaTime);

    if (finished)
    {
        m_currentMove.reset();
        m_currentIndex = (m_currentIndex + 1) % m_waypoints.size();
    }

    return false;
}

#include "AI/MoveToCommand.h"

bool MoveToCommand::execute(AIController& ai, float deltaTime)
{
    if (!m_pathComputed)
    {
        m_path = m_navMesh.findPath(ai.getOwnerPosition(), m_targetPos);
        m_pathComputed = true;
        m_currentWaypoint = 0;

        if (m_path.empty())
            return true;
    }

    if (m_currentWaypoint >= m_path.size())
        return true;

    glm::vec3 currentPos = ai.getOwnerPosition();
    glm::vec3 waypoint = m_path[m_currentWaypoint];
    glm::vec3 toWaypoint = waypoint - currentPos;
    toWaypoint.y = 0.0f;

    float distance = glm::length(toWaypoint);

    if (distance < m_arriveThreshold)
    {
        m_currentWaypoint++;
        return m_currentWaypoint >= m_path.size();
    }

    glm::vec3 direction = toWaypoint / distance;
    ai.moveTowards(direction, m_speed);
    ai.faceDirection(direction);

    return false;
}

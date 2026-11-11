#include "AI/AIController.h"
#include "AI/ICommand.h"

void AIController::updateVision()
{
    m_canSeeTarget = false;

    if (!m_visionTarget)
    {
        LOG("updateVision: no vision target set!");
        return;
    }

    glm::vec3 eyePos = m_owner->transform.position + m_eyeOffset;
    glm::vec3 targetPos = m_visionTarget->transform.position;

    glm::vec3 toTarget = targetPos - eyePos;
    float distance = glm::length(toTarget);

    if (distance > m_visionRange)
    {
        LOG("updateVision: target too far, distance=" << distance << " range=" << m_visionRange);
        return;
    }

    glm::vec3 direction = toTarget / distance;

    float yawRad = glm::radians(m_owner->transform.rotation.y);
    glm::vec3 forward(sin(yawRad), 0.0f, -cos(yawRad));

    float facingDot = glm::dot(glm::normalize(glm::vec3(direction.x, 0.0f, direction.z)), forward);
    if (facingDot < m_visionFOVCos)
    {
        LOG("updateVision: target outside FOV, dot=" << facingDot << " required=" << m_visionFOVCos);
        return;
    }

    RaycastHit hit = m_physics.raycast(eyePos, direction, distance);
    bool blocked = hit.hit && hit.distance < distance - 0.2f;

    LOG("updateVision: distance=" << distance << " facingDot=" << facingDot << " blocked=" << blocked);

    if (!blocked)
    {
        m_canSeeTarget = true;
        m_lastKnowTargetPos = targetPos;
    }
}


void AIController::pushCommand(std::unique_ptr<ICommand> command)
{
    m_commandQueue.push_back(std::move(command));
}

void AIController::clearCommands()
{
    m_commandQueue.clear();
}

void AIController::updateCommands(float deltaTime)
{
    if (m_commandQueue.empty()) return;

    ICommand* current = m_commandQueue.front().get();
    bool finished = current->execute(*this, deltaTime);

    if (finished)
        m_commandQueue.erase(m_commandQueue.begin());
}

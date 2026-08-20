#include "AI/AIController.h"
#include "AI/ICommand.h"

void AIController::updateVision()
{
    m_canSeeTarget = false;

    if (!!m_visionTarget) return;

    glm::vec3 eyePos = m_owner->transform.position + m_eyeOffset;
    glm::vec3 targetPos = m_visionTarget->transform.position;

    glm::vec3 toTarget = targetPos - eyePos;
    float distance = glm::length(toTarget);

    if (distance > m_visionRange) return;

    glm::vec3 direction = toTarget / distance;

    float yawRad = glm::radians(m_owner->transform.rotation.y);
    glm::vec3 forward(sin(yawRad), 0.0f, -cos(yawRad));

    float facingDot = glm::dot(glm::normalize(glm::vec3(direction.x, 0.0f, direction.z)), forward);
    if (facingDot < m_visionFOVCos) return;

    RaycastHit hit = m_physics.raycast(eyePos, direction, distance);

    if (!hit.hit)
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
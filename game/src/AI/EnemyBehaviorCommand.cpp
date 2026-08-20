#include "AI/EnemyBehaviorCommand.h"

bool EnemyBehaviorCommand::execute(AIController& ai, float deltaTime)
{
    if (ai.canSeeTarget())
    {
        m_loseVisionTimer = m_loseVisionGracePeriod;
    }
    else if (m_loseVisionTimer > 0.0f)
    {
        m_loseVisionTimer -= deltaTime;
    }

    bool effectivelyChasing = m_loseVisionTimer > 0.0f;

    if (effectivelyChasing)
    {
        m_patrolState.reset();

        if (!m_chaseMove || m_repathTimer <= 0.0f)
        {
            m_chaseMove = std::make_unique<MoveToCommand>(m_navMesh, ai.getLastKnowTargetPosition(), m_chaseSpeed);
            m_repathTimer = m_repathInterval;
        }
        m_repathTimer -= deltaTime;
        m_chaseMove->execute(ai, deltaTime);
    }
    else
    {
        m_chaseMove.reset();
        if (!m_patrolState)
            m_patrolState = std::make_unique<PatrolCommand>(m_navMesh, m_patrolPoints, m_ptarolSpeed);
        m_patrolState->execute(ai, deltaTime);
    }
    return false;
}
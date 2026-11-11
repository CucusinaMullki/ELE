#pragma once
#include "AI/ICommand.h"
#include "AI/PatrolCommand.h"
#include "AI/MoveToCommand.h"
#include "Scene/GameObject.h"
#include "Nav/NavMesh.h"

class EnemyBehaviorCommand : public ICommand
{
public:
    EnemyBehaviorCommand(const NavMesh& navMesh, std::vector<glm::vec3> patrolPoints, GameObject* target)
        : m_navMesh(navMesh), m_patrolPoints(std::move(patrolPoints)), m_target(target)
    {}

    bool execute(AIController& ai, float deltaTime) override;

private:
    const NavMesh& m_navMesh;
    std::vector<glm::vec3> m_patrolPoints;
    GameObject* m_target;

    std::unique_ptr<PatrolCommand> m_patrolState;
    std::unique_ptr<MoveToCommand> m_chaseMove;

    float m_ptarolSpeed = 1.5f;
    float m_chaseSpeed = 2.5f;
    float m_repathInterval = 0.5f;
    float m_repathTimer = 0.0f;
    float m_loseVisionTimer = 0.0f;
    float m_loseVisionGracePeriod = 1.0f;
};

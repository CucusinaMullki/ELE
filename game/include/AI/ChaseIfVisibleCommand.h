#pragma once
#include "AI/ICommand.h"
#include "AI/AIController.h"
#include "Core/Log.h"

class ChaseIfVisibleCommand : public ICommand
{
public:
    bool execute(AIController& ai, float deltaTime) override
    {
        if (ai.canSeeTarget())
        {
            LOG("AI: I see the target at last known position!");
        }
        return false;
    }
};
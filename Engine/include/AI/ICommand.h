#pragma once

class AIController;

class ICommand
{
public:
    virtual ~ICommand() = default;

    virtual bool execute(AIController& ai, float deltaTime) = 0;
};

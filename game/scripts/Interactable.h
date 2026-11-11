#pragma once
#include "Scripting/Script.h"
#include "Scene/GameObject.h"

class Interactable : public Script
{
public:
    float interactDistance = 2.5f;

    virtual void interact(GameObject& who) = 0;
    virtual const char* prompt() const { return "Use"; }
};

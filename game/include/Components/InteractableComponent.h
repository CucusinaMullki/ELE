#pragma once
#include "Scene/IComponent.h"
#include <functional>
#include <string>

class InteractableComponent : public IComponent
{
public:
    std::function<void()> onInteract;
};

#pragma once
#include "Scene/Property.h"

class GameObject;

class IComponent
{
public:
    virtual ~IComponent() = default;

    virtual void onAttach(GameObject& owner) {}
    virtual void update(float deltaTime) {}
    virtual void fixedUpdate(float fixedDeltaTime) {}

    virtual const char* typeName() const { return nullptr; }
    virtual void visit(PropertyVisitor&) {}
    virtual void onPropertiesChanged() {}
};

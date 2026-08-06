#pragma once

class GameObject;

class IComponent
{
public:
    virtual ~IComponent() = default;

    virtual void onAttach(GameObject& owner) {}

    virtual void update(float deltaTime) {}

    virtual void fixedUpdate(float fixedDeltaTime) {}
};
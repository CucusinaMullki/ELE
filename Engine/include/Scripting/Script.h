#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"

class Engine;

class Script : public IComponent
{
public:
    ~Script() override;

    void onAttach(GameObject& owner) override { m_owner = &owner; }
    void update(float dt) override;
    void fixedUpdate(float dt) override { onFixedUpdate(dt); }

    virtual void onStart() {}
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onFixedUpdate(float dt) { (void)dt; }

    GameObject& owner() const { return *m_owner; }
    Transform& transform() const { return m_owner->transform; }

    static Engine& engine();
    static void setEngine(Engine* e);

protected:
    GameObject* m_owner = nullptr;
    bool m_started = false;
};

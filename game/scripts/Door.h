#pragma once
#include "Interactable.h"
#include "Scripting/ScriptModule.h"
#include "Components/AudioComponent.h"
#include <algorithm>

class Door : public Interactable
{
public:
    ELE_SCRIPT(Door)

    float openAngle = 100.0f;
    float speed     = 180.0f;
    bool  locked    = false;
    float soundVolume = 0.6f;

    void visit(PropertyVisitor& v) override
    {
        v.field("openAngle", openAngle);
        v.field("speed", speed);
        v.field("locked", locked);
        v.field("soundVolume", soundVolume);
        v.field("interactDistance", interactDistance);
    }

    void onStart() override
    {
        m_closedYaw = transform().rotation.y;
        m_open = false;
    }

    void interact(GameObject&) override
    {
        if (locked) return;
        m_open = !m_open;

        if (auto* audio = owner().getComponent<AudioComponent>())
            audio->play(soundVolume);
    }

    const char* prompt() const override { return locked ? "Locked" : (m_open ? "Close" : "Open"); }

    void onUpdate(float dt) override
    {
        const float target = m_closedYaw + (m_open ? openAngle : 0.0f);
        float& yaw = transform().rotation.y;
        const float step = speed * dt;
        yaw += std::clamp(target - yaw, -step, step);
    }

private:
    float m_closedYaw = 0.0f;
    bool  m_open = false;
};

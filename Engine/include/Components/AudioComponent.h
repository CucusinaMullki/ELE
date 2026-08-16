#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Audio/AudioSystem.h"
#include <string>

class AudioComponent : public IComponent
{
public:
    AudioComponent(AudioSystem& audio, PhysicsSystem& physics, std::string soundName)
        : m_audio(audio), m_physics(physics), m_soundName(std::move(soundName))
    {}

    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void update(float deltaTime) override
    {
        if (!m_isPlaying) return;

        m_audio.updateSoundPosition(m_soundName, m_owner->transform.position);

        float occlusion = calculateOcclusion();
        m_audio.setOcclusion(m_soundName, occlusion);
    }

    void play(float volume = 1.0f)
    {
        m_audio.playSound3D(m_soundName, m_owner->transform.position, volume);
        m_isPlaying = true;
    }

    void stop()
    {
        m_audio.stopSound(m_soundName);
        m_isPlaying = false;
    }

private:
    float calculateOcclusion() const
    {
        glm::vec3 soundPos = m_owner->transform.position;
        glm::vec3 listenerPos = m_audio.getListenerWorldPosition();
        glm::vec3 toListener = listenerPos - soundPos;
        float distance = glm::length(toListener);

        if (distance < 0.01f) return 0.0f;

        glm::vec3 direction = toListener / distance;

        RaycastHit hit = m_physics.raycast(soundPos, direction, distance);

        return hit.hit ? 1.0f : 0.0f;
    }

    AudioSystem& m_audio;
    PhysicsSystem& m_physics;
    GameObject* m_owner = nullptr;
    std::string m_soundName;
    bool m_isPlaying = false;
};
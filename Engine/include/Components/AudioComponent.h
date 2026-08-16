#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Audio/AudioSystem.h"
#include <string>

class AudioComponent : public IComponent
{
public:
    AudioComponent(AudioSystem& audio, std::string soundName)
        : m_audio(audio), m_soundName(std::move(soundName))
    {}

    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void update(float deltaTime) override
    {
        if (m_isPlaying)
            m_audio.updateSoundPosition(m_soundName, m_owner->transform.position);
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
    AudioSystem& m_audio;
    GameObject* m_owner = nullptr;
    std::string m_soundName;
    bool m_isPlaying = false;
};
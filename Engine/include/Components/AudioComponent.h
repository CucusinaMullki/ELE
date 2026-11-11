#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Audio/AudioSystem.h"
#include "Physics/PhysicsSystem.h"
#include "Core/Path.h"
#include <cstdint>
#include <filesystem>
#include <string>

class AudioComponent : public IComponent
{
public:
    std::string sound;
    float volume = 1.0f;
    bool loop = false;
    bool playOnStart = true;
    bool spatial = true;
    bool occlusion = true;

    AudioComponent(AudioSystem& audio, PhysicsSystem& physics, std::string soundPathOrName = "")
        : sound(std::move(soundPathOrName)), m_audio(audio), m_physics(physics) {}

    ~AudioComponent() override { release(); }

    const char* typeName() const override { return "Audio"; }

    void visit(PropertyVisitor& v) override
    {
        v.field("sound", sound);
        v.field("volume", volume);
        v.field("loop", loop);
        v.field("playOnStart", playOnStart);
        v.field("spatial", spatial);
        v.field("occlusion", occlusion);
    }

    void onAttach(GameObject& owner) override
    {
        m_owner = &owner;
        reload();
    }

    void onPropertiesChanged() override
    {
        const bool wasPlaying = m_isPlaying;
        stop();
        reload();
        if (wasPlaying) play();
    }

    void update(float) override
    {
        if (playOnStart && !m_started)
        {
            m_started = true;
            play();
        }
        if (!m_isPlaying || !spatial || m_key.empty()) return;

        m_audio.updateSoundPosition(m_key, m_owner->transform.position);
        if (occlusion)
            m_audio.setOcclusion(m_key, calculateOcclusion());
    }

    void play() { play(volume); }

    void play(float vol)
    {
        if (m_key.empty()) return;
        if (spatial) m_audio.playSound3D(m_key, m_owner->transform.position, vol);
        else m_audio.playSound(m_key, vol);
        m_isPlaying = true;
    }

    void stop()
    {
        if (!m_key.empty() && m_isPlaying) m_audio.stopSound(m_key);
        m_isPlaying = false;
    }

    bool isPlaying() const { return m_isPlaying; }
    const std::string& soundKey() const { return m_key; }

private:
    void reload()
    {
        release();
        if (sound.empty()) return;

        const std::filesystem::path file = Paths::assetDir() / sound;
        std::error_code ec;
        if (std::filesystem::is_regular_file(file, ec))
        {
            m_key = sound + "#" + std::to_string((uintptr_t)this);
            m_owned = m_audio.loadSound(m_key, file.string());
            if (!m_owned) { m_key.clear(); return; }
        }
        else if (m_audio.isLoaded(sound))
        {
            m_key = sound;
            m_owned = false;
        }
        else
            return;

        m_audio.setLooping(m_key, loop);
    }

    void release()
    {
        if (m_owned && !m_key.empty())
        {
            m_audio.stopSound(m_key);
            m_audio.unloadSound(m_key);
        }
        m_key.clear();
        m_owned = false;
        m_isPlaying = false;
    }

    float calculateOcclusion() const
    {
        glm::vec3 soundPos = m_owner->transform.position;
        glm::vec3 toListener = m_audio.getListenerWorldPosition() - soundPos;
        float distance = glm::length(toListener);
        if (distance < 0.01f) return 0.0f;
        RaycastHit hit = m_physics.raycast(soundPos, toListener / distance, distance);
        return hit.hit ? 1.0f : 0.0f;
    }

    AudioSystem& m_audio;
    PhysicsSystem& m_physics;
    GameObject* m_owner = nullptr;
    std::string m_key;
    bool m_owned = false;
    bool m_isPlaying = false;
    bool m_started = false;
};

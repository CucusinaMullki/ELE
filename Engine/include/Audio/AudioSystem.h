#pragma once
#include "miniaudio.h"
#include <string>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>

class AudioSystem
{
public:
    void initialize();
    void cleanup();

    bool loadSound(const std::string& name, const std::string& path);

    void playSound(const std::string& name, float volume = 1.0f);
    void playSound3D(const std::string& name, glm::vec3 position, float volume = 1.0);

    void setListenerPosition(glm::vec3 position, glm::vec3 forwarwd, glm::vec3 up);
    void setOcclusion(const std::string& name, float occlusionFactor);

    void updateSoundPosition(const std::string& name, glm::vec3 position);
    void stopSound(const std::string& name);

    void setListenerWorldPosition(glm::vec3 pos) { m_listenerPos = pos; }
    glm::vec3 getListenerWorldPosition() const { return m_listenerPos; }

private:
    bool m_ready = true;
    glm::vec3 m_listenerPos{0.0f};
    ma_context m_context;
    ma_engine m_engine;
    std::unordered_map<std::string, std::unique_ptr<ma_sound>> m_sounds;
};
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

private:
    ma_context m_context;
    ma_engine m_engine;
    std::unordered_map<std::string, std::unique_ptr<ma_sound>> m_sounds;
};
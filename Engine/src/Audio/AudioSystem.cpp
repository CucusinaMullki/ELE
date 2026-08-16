#include "Audio/AudioSystem.h"
#include "Core/Log.h"
#include "Core/Path.h"

void AudioSystem::initialize()
{
#ifdef _WIN32
    ma_backend backends[] = { ma_backend_wasapi, ma_backend_dsound, ma_backend_winmm };
#else
    ma_backend backends[] = { ma_backend_pulseaudio, ma_backend_alsa };
#endif

    ma_result ctxResult = ma_context_init(backends, 2, nullptr, &m_context);
    LOG("ma_context_init result=" << (int)ctxResult);
    if (ctxResult == MA_SUCCESS)
        LOG("selected backend=" << (int)m_context.backend);

    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.pContext = &m_context;

    ma_result result = ma_engine_init(&engineConfig, &m_engine);
    if (result != MA_SUCCESS)
    {
        LOG("ma_engine_init FAILED, result=" << (int)result);
        throw std::runtime_error("failed to initialize miniaudio engine");
    }
    LOG("ma_engine_init succeeded");

    ma_device* device = ma_engine_get_device(&m_engine);
    if (device)
        LOG("audio device name: " << device->playback.name);
}

void AudioSystem::cleanup()
{
    m_sounds.clear();
    ma_engine_uninit(&m_engine);
    ma_context_uninit(&m_context);
}

bool AudioSystem::loadSound(const std::string& name, const std::string& path)
{
    if (m_sounds.find(name) != m_sounds.end())
        return true;
    
    std::string fullPath = (Paths::assetDir() / "assets" / path).string();

    auto sound = std::make_unique<ma_sound>();
    ma_result result = ma_sound_init_from_file(&m_engine, fullPath.c_str(), 0, nullptr, nullptr, sound.get());

    if (result != MA_SUCCESS)
    {
        LOG("failed to load sound: " << fullPath);
        return false;
    }

    m_sounds[name] = std::move(sound);
    return true;
}

void AudioSystem::playSound(const std::string& name, float volume)
{
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return;

    ma_sound_set_volume(it->second.get(), volume);
    ma_sound_seek_to_pcm_frame(it->second.get(), 0);
    ma_sound_start(it->second.get());
}

void AudioSystem::playSound3D(const std::string& name, glm::vec3 position, float volume)
{
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return;

    ma_sound_set_position(it->second.get(), position.x, position.y, position.z);
    ma_sound_set_volume(it->second.get(), volume);
    ma_sound_seek_to_pcm_frame(it->second.get(), 0);
    ma_sound_start(it->second.get());
}

void AudioSystem::updateSoundPosition(const std::string& name, glm::vec3 position)
{
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return;
    ma_sound_set_position(it->second.get(), position.x, position.y, position.z);
}

void AudioSystem::stopSound(const std::string& name)
{
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return;
    ma_sound_stop(it->second.get());
}

void AudioSystem::setOcclusion(const std::string& name, float occlusionFactor)
{
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return;

    float volumeMultiplier = 1.0f - occlusionFactor * 0.7f;
    ma_sound_set_volume(it->second.get(), volumeMultiplier);

    if (occlusionFactor > 0.01f)
    {
        double cutoffFrequency = 8000.0 - occlusionFactor * 6500.0;
        ma_lpf_node_config lpfConfig = ma_lpf_node_config_init(1, m_engine.sampleRate, cutoffFrequency, 2);
    }
}

void AudioSystem::setListenerPosition(glm::vec3 position, glm::vec3 forward, glm::vec3 up)
{
    ma_engine_listener_set_position(&m_engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&m_engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_engine, 0, up.x, up.y, up.z);
}
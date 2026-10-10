#pragma once

#include "Vulkan/Particles.h"
#include <glm/glm.hpp>
#include <string>

class ParticlesComponent
{
public:
    bool bind(Particles& particles, const std::string& effectName)
    {
        m_particles = &particles;
        m_index = particles.findEmitter(effectName);
        return m_index >= 0;
    }

    bool isBound() const { return m_particles && m_index >= 0; }
    bool isActive() const { auto* e = emitter(); return e && e->active; }

    void play() { if (auto* e = emitter()) { e->startTime = m_particles->now(); e->active = true; } }
    void stop() { if (auto* e = emitter()) e->active = false; }
    void setActive(bool a) { if (auto* e = emitter()) e->active = a; }
    void setPosition(const glm::vec3& p) { if (auto* e = emitter()) e->offset = p; }

private:
    Particles::Emitter* emitter() const { return m_particles ? m_particles->getEmitter(m_index) : nullptr; }

    Particles* m_particles = nullptr;
    int m_index = -1;
};
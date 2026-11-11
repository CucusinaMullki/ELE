#include "Scripting/Script.h"
#include <cassert>

namespace { Engine* g_scriptEngine = nullptr; }

Script::~Script() = default;

void Script::update(float dt)
{
    if (!m_started)
    {
        m_started = true;
        onStart();
    }
    onUpdate(dt);
}

Engine& Script::engine()
{
    assert(g_scriptEngine && "Script::setEngine not called");
    return *g_scriptEngine;
}

void Script::setEngine(Engine* e) { g_scriptEngine = e; }

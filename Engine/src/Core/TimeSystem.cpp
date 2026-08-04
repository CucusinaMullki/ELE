#include "Core/TimeSystem.h"

void TimeSystem::update(float rawDeltaTime)
{
    m_rawDeltaTime = rawDeltaTime;

    if (!m_paused)
    {
        m_totalTime += rawDeltaTime;
        m_accumulator += rawDeltaTime * m_timeScale;
    }
}

int TimeSystem::consumeFixedSteps()
{
    int steps = 0;
    while (m_accumulator >= m_fixedDeltaTime && steps < MAX_FIXED_STEPS_PER_FRAME)
    {
        m_accumulator -= m_fixedDeltaTime;
        steps++;
    }

    if (steps == MAX_FIXED_STEPS_PER_FRAME)
        m_accumulator = 0.0f;

    return steps;
}
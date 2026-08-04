#pragma once

class TimeSystem
{
public:
    void update(float rawDeltaTime);

    float deltaTime() const { return m_paused ? 0.0f : m_rawDeltaTime * m_timeScale; }
    float unscaledDeltaTime() const { return m_rawDeltaTime; }

    float fixedDeltaTime() const { return m_fixedDeltaTime; }
    void setFixedDeltaTime(float dt) { m_fixedDeltaTime = dt; }

    void setTimeScale(float scale) { m_timeScale = scale; }
    float getTimeScale() const { return m_timeScale; }

    void pause() { m_paused = true; }
    void resume() { m_paused = false; }
    bool isPaused() const { return m_paused; }

    double totalTime() const { return m_totalTime; }

    int consumeFixedSteps();

private:
    float m_rawDeltaTime = 0.0f;
    float m_timeScale = 1.0f;
    float m_fixedDeltaTime = 1.0f / 60.0f;
    float m_accumulator = 0.0f;
    double m_totalTime = 0.0;
    bool m_paused = false;

    static constexpr int MAX_FIXED_STEPS_PER_FRAME = 5;
};
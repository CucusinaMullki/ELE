#pragma once
#include "Scene/IComponent.h"
#include "Core/Input.h"
#include "UI/DebugConsoleWidget.h"

class DebugConsoleController : public IComponent
{
public:
    DebugConsoleController(Input& input, DebugConsoleWidget& console)
        : m_input(input), m_console(console)
    {}

    void update(float deltaTime) override
    {
        if (m_input.isActionJustPressed("ToggleConsole"))
            m_console.setVisible(!m_console.isVisible());
    }

private:
    Input& m_input;
    DebugConsoleWidget& m_console;
};

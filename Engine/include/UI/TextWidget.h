#pragma once
#include "UI/Widget.h"
#include <imgui.h>
#include <string>

class TextWidget : public Widget
{
public:
    TextWidget(std::string text) : m_text(std::move(text)) {}

    void draw() override
    {
        ImGui::SetNextWindowPos(ImVec2(m_position.x, m_position.y));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin(("##text_" + std::to_string((uintptr_t)this)).c_str(), nullptr, flags);
        ImGui::SetWindowFontScale(m_fontScale);
        ImGui::TextColored(ImVec4(m_color.r, m_color.g, m_color.b, m_color.a), "%s", m_text.c_str());
        ImGui::End();
    
        ImGui::PopStyleVar();
    }

    void setText(const std::string& text) { m_text = text; }
    const std::string& getText() const { return m_text; }

    void setColor(glm::vec4 color) { m_color = color; }

    void setFontScale(float scale) { m_fontScale = scale; }
    float getFontScale() const { return m_fontScale; }

private:
    std::string m_text;
    glm::vec4 m_color{1.0f, 1.0f, 1.0f, 1.0f};
    float m_fontScale = 1.0f;
};
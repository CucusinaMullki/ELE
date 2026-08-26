#pragma once
#include "UI/Widget.h"
#include <imgui.h>
#include <algorithm>

class ProgressBarWidget : public Widget
{
public:
    ProgressBarWidget(float initialValue = 0.0f) : m_value(initialValue) {}
    
    void draw() override
    {
        ImGui::SetNextWindowPos(ImVec2(m_position.x, m_position.y));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        
        ImGui::Begin(("##progress_" + std::to_string((uintptr_t)this)).c_str(), nullptr, flags);

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(m_fillColor.r, m_fillColor.g, m_fillColor.b, m_fillColor.a));
        ImGui::ProgressBar(m_value, ImVec2(m_size.x, m_size.y), m_showLabel ? nullptr : "");
        ImGui::PopStyleColor();

        ImGui::End();
    }

    void setValue(float value) { m_value = std::clamp(value, 0.0f, 1.0f); }
    float getValue() const { return m_value; }

    void setFillColor(glm::vec4 color) { m_fillColor = color; }
    void setShowLabel(bool show) { m_showLabel = show; }

private:
    float m_value = 0.0f;
    glm::vec4 m_fillColor{0.2f, 0.7f, 0.2f, 1.0f};
    bool m_showLabel = true;
};
#pragma once
#include "UI/Widget.h"
#include "UI/DebugConsole.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cctype>

class DebugConsoleWidget : public Widget
{
public:
    void draw() override
    {
        ImGui::SetNextWindowPos(ImVec2(m_position.x, m_position.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(m_size.x, m_size.y), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Debug Console", &m_open))
        {
            ImGui::End();
            return;
        }

        ImGui::InputText("Filter", m_filterBuffer, sizeof(m_filterBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            DebugConsole::instance().clear();

        ImGui::Separator();

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::string filter = m_filterBuffer;
        std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return std::tolower(c); });

        for (const auto& entry : DebugConsole::instance().getEntries())
        {
            if (!filter.empty())
            {
                std::string lowerEntry = entry;
                std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(), [](unsigned char c) { return std::tolower(c); });

                if (lowerEntry.find(filter) == std::string::npos)
                    continue;
            }

            ImGui::TextUnformatted(entry.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }

    bool* getOpenFlag() { return &m_open; }
    
private:
    bool m_open = true;
    char m_filterBuffer[128] = "";
};
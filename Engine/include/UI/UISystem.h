#pragma once
#include "UI/Widget.h"
#include <vector>
#include <memory>

class UISystem
{
public:
    template<typename T, typename... Args>
    T& addWidget(Args&&... args)
    {
        auto widget = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *widget;
        m_widgets.push_back(std::move(widget));
        return ref;
    }

    void removeAllWidgets() { m_widgets.clear(); }

    void draw()
    {
        for (auto& widget : m_widgets)
        {
            if (widget->isVisible())
                widget->draw();
        }
    }

private:
    std::vector<std::unique_ptr<Widget>> m_widgets;
};
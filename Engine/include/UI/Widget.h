#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

class Widget
{
public:
    virtual ~Widget() = default;

    virtual void draw() = 0;

    void setPosition(glm::vec2 pos) { m_position = pos; }
    glm::vec2 getPosition() const { return m_position; }

    void setSize(glm::vec2 size) { m_size = size; }
    glm::vec2 getSize() const { return m_size; }

    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

protected:
    glm::vec2 m_position{0.0f};
    glm::vec2 m_size{100.0f, 30.0f};
    bool m_visible = true;
};

#pragma once
#include <glm/glm.hpp>

enum class LightType : int
{
    Directional = 0,
    Point = 1
};

struct Light
{
    glm::vec4 position;
    glm::vec4 color;
};
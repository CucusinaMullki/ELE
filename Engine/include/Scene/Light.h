#pragma once
#include <glm/glm.hpp>

enum class LightType : int
{
    Directional = 0,
    Point = 1,
    Spot = 2,
    Rect = 3,
};

struct Light
{
    glm::vec4 position;
    glm::vec4 color;
    glm::vec4 direction{0.0f};
    glm::vec4 params{0.0f};
    glm::vec4 right{0.0f};

    LightType type() const { return (LightType)(int)(position.w + 0.5f); }
};

static_assert(sizeof(Light) == 80, "Light must match light.glsl (5 x vec4)");

#pragma once
#include <glm/glm.hpp>

struct GodRaySettings
{
    bool enabled = false;

    float intensity = 1.0f;
    float decay = 0.95f;
    float density = 0.9f;
    float weight = 0.4f;
    float sunSize = 0.9995f;
    int sampleCount = 64;
};

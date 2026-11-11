#pragma once
#include <glm/glm.hpp>

struct FogSettings
{
    bool enabled = false;

    glm::vec3 scatteringColor{0.7f, 0.75f, 0.85f};
    float density = 0.04f;
    float heightFalloff = 0.15f;
    float heightStart = 0.0f;
    float maxDistance = 60.0f;
    int stepCount = 24;
    float ambientIntensity = 0.15f;
    float sunIntensity = 1.0f;
    float noiseScale = 0.15f;
    float noiseSpeed = 0.02f;
};

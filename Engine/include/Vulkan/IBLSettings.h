#pragma once
#include <glm/glm.hpp>

struct IBLSettings
{
    bool enabled = true;
    float diffuseIntensity = 1.0f;
    float specularIntensity = 1.0f;
    glm::vec3 tint = glm::vec3(1.0f);
};

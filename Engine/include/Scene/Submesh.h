#pragma once
#include <cstdint>
#include <glm/glm.hpp>

struct Submesh
{
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int materialIndex = 0;

    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};
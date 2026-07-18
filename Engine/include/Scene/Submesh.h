#pragma once
#include <cstdint>

struct Submesh
{
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int materialIndex = 0;
};
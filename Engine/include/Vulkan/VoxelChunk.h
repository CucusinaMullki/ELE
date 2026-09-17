#pragma once
#include "ASH/ASH.h"
#include "Vulkan/DynamicVoxelProxy.h"
#include <glm/glm.hpp>
#include <functional>

struct ChunkCoord
{
    int32_t x, y, z;

    bool operator==(const ChunkCoord& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct ChunkCoordHash
{
    size_t operator()(const ChunkCoord& c) const
    {
        size_t h = std::hash<int32_t>()(c.x);
        h ^= std::hash<int32_t>()(c.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int32_t>()(c.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct VoxelChunk
{
    ChunkCoord coord{};
    int32_t atlasSlotIndex = -1;
    bool needsRebake = true;
};

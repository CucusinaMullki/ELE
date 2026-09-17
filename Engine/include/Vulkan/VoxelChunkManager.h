#pragma once
#include "Vulkan/VoxelChunk.h"
#include <unordered_map>
#include <vector>

class VoxelChunkManager
{
public:
    void initialize(float chunkSize, int32_t visibilityRadiusChunks, int32_t atlasSlotsPerAxis);

    struct UpdateResult
    {
        std::vector<ChunkCoord> chunksToBake;
        std::vector<ChunkCoord> chunksToEvict;
    };
    UpdateResult update(glm::vec3 cameraPosition);

    ChunkCoord worldToChunkCoord(glm::vec3 worldPos) const;
    glm::vec3 chunkCoordToWorldMin(ChunkCoord coord) const;

    float getChunkSize() const { return m_chunkSize; }
    int32_t getAtlasSlotsPerAxis() const { return m_atlasSlotsPerAxis; }
    int32_t getMaxAtlasSlots() const { return m_atlasSlotsPerAxis * m_atlasSlotsPerAxis * m_atlasSlotsPerAxis; }

    const std::unordered_map<ChunkCoord, VoxelChunk, ChunkCoordHash>& getActiveChunks() const { return m_activeChunks; }
    VoxelChunk* getChunk(ChunkCoord coord);

private:
    int32_t findFreeAtlasSlot();

    float m_chunkSize = 8.0f;
    int32_t m_visibilityRadiusChunks = 2;
    int32_t m_atlasSlotsPerAxis = 5;

    std::unordered_map<ChunkCoord, VoxelChunk, ChunkCoordHash> m_activeChunks;
    std::vector<bool> m_atlasSlotOccupied;
};
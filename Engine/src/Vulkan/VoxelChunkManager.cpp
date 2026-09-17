#include "Vulkan/VoxelChunkManager.h"
#include <algorithm>
#include <cmath>

void VoxelChunkManager::initialize(float chunkSize, int32_t visibilityRadiusChunks, int32_t atlasSlotsPerAxis)
{
    m_chunkSize = chunkSize;
    m_visibilityRadiusChunks = visibilityRadiusChunks;
    m_atlasSlotsPerAxis = atlasSlotsPerAxis;
    m_atlasSlotOccupied.assign(getMaxAtlasSlots(), false);
}

ChunkCoord VoxelChunkManager::worldToChunkCoord(glm::vec3 worldPos) const
{
    return ChunkCoord{
        (int32_t)std::floor(worldPos.x / m_chunkSize),
        (int32_t)std::floor(worldPos.y / m_chunkSize),
        (int32_t)std::floor(worldPos.z / m_chunkSize)
    };
}

glm::vec3 VoxelChunkManager::chunkCoordToWorldMin(ChunkCoord coord) const
{
    return glm::vec3((float)coord.x, (float)coord.y, (float)coord.z) * m_chunkSize;
}

int32_t VoxelChunkManager::findFreeAtlasSlot()
{
    for (size_t i = 0; i < m_atlasSlotOccupied.size(); i++)
    {
        if (!m_atlasSlotOccupied[i])
        {
            m_atlasSlotOccupied[i] = true;
            return (int32_t)i;
        }
    }
    return -1;
}

VoxelChunkManager::UpdateResult VoxelChunkManager::update(glm::vec3 cameraPosition)
{
    UpdateResult result;

    ChunkCoord centerCoord = worldToChunkCoord(cameraPosition);

    std::unordered_map<ChunkCoord, bool, ChunkCoordHash> desiredChunks;
    for (int32_t dx = -m_visibilityRadiusChunks; dx <= m_visibilityRadiusChunks; dx++)
    for (int32_t dy = -m_visibilityRadiusChunks; dy <= m_visibilityRadiusChunks; dy++)
    for (int32_t dz = -m_visibilityRadiusChunks; dz <= m_visibilityRadiusChunks; dz++)
    {
        ChunkCoord c{ centerCoord.x + dx, centerCoord.y + dy, centerCoord.z + dz };
        desiredChunks[c] = true;
    }

    for (auto it = m_activeChunks.begin(); it != m_activeChunks.end(); )
    {
        if (desiredChunks.find(it->first) == desiredChunks.end())
        {
            if (it->second.atlasSlotIndex >= 0)
                m_atlasSlotOccupied[it->second.atlasSlotIndex] = false;

            result.chunksToEvict.push_back(it->first);
            it = m_activeChunks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    std::vector<ChunkCoord> sortedCoords;
    for (const auto& [coord, _] : desiredChunks)
        sortedCoords.push_back(coord);

    std::sort(sortedCoords.begin(), sortedCoords.end(), [&](const ChunkCoord& a, const ChunkCoord& b)
    {
        int32_t da = (a.x - centerCoord.x) * (a.x - centerCoord.x) + (a.y - centerCoord.y) * (a.y - centerCoord.y) + (a.z - centerCoord.z) * (a.z - centerCoord.z);
        int32_t db = (b.x - centerCoord.x) * (b.x - centerCoord.x) + (b.y - centerCoord.y) * (b.y - centerCoord.y) + (b.z - centerCoord.z) * (b.z - centerCoord.z);
        return da < db;
    });

    for (const auto& coord : sortedCoords)
    {
        if (!result.chunksToBake.empty())
            break;

        if (m_activeChunks.find(coord) == m_activeChunks.end())
        {
            int32_t slot = findFreeAtlasSlot();
            if (slot < 0)
                continue;

            VoxelChunk chunk{};
            chunk.coord = coord;
            chunk.atlasSlotIndex = slot;
            chunk.needsRebake = true;

            m_activeChunks[coord] = chunk;
            result.chunksToBake.push_back(coord);
        }
    }

    return result;
}

VoxelChunk* VoxelChunkManager::getChunk(ChunkCoord coord)
{
    auto it = m_activeChunks.find(coord);
    return (it != m_activeChunks.end()) ? &it->second : nullptr;
}
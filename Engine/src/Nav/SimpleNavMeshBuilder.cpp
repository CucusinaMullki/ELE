#include "Nav/SimpleNavMeshBuilder.h"
#include "Nav/GeometryParser.h"
#include "Core/Log.h"
#include <unordered_map>

NavMesh SimpleNavMeshBuilder::build(PhysicsSystem& physics, glm::vec3 volumeMin, glm::vec3 volumeMax, float cellSize)
{
    NavMesh mesh;

    int stepsX = (int)((volumeMax.x - volumeMin.x) / cellSize) + 1;
    int stepsZ = (int)((volumeMax.z - volumeMin.z) / cellSize) + 1;

    std::unordered_map<int64_t, int> gridToNodeId;
    auto gridKey = [stepsX](int ix, int iz) -> int64_t { return (int64_t)iz * stepsX + ix; };

    int walkableCount = 0;

    for (int iz = 0; iz < stepsZ; iz++)
    {
        for (int ix = 0; ix < stepsX; ix++)
        {
            float worldX = volumeMin.x + ix * cellSize;
            float worldZ = volumeMin.z + iz * cellSize;

            glm::vec3 checkPoint(worldX, (volumeMin.y + volumeMax.y) * 0.5f, worldZ);

            WalkabilityResult walkability = GeometryParser::isWalkable(physics, checkPoint);

            if (walkability.walkable)
            {
                int nodeId = mesh.addNode(walkability.groundPoint);
                gridToNodeId[gridKey(ix, iz)] = nodeId;
                walkableCount++;
            }
        }
    }

    LOG("SimpleNavMeshBuilder: " << walkableCount << " walkable nodes out of " << (stepsX * stepsZ) << " grid points checked");

    int connectionCount = 0;
    for (int iz = 0; iz < stepsZ; iz++)
    {
        for (int ix = 0; ix < stepsX; ix++)
        {
            auto it = gridToNodeId.find(gridKey(ix, iz));
            if (it == gridToNodeId.end()) continue;
            int currentId = it->second;

            if (ix + 1 < stepsX)
            {
                auto rightIt = gridToNodeId.find(gridKey(ix + 1, iz));
                if (rightIt != gridToNodeId.end())
                {
                    const auto& nodes = mesh.getNodes();
                    if (hasLineOfSight(physics, nodes[currentId].position, nodes[rightIt->second].position))
                    {
                        mesh.connect(currentId, rightIt->second);
                        connectionCount++;
                    }
                }
            }

            if (iz + 1 < stepsZ)
            {
                auto downIt = gridToNodeId.find(gridKey(ix, iz + 1));
                if (downIt != gridToNodeId.end())
                {
                    const auto& nodes = mesh.getNodes();
                    if (hasLineOfSight(physics, nodes[currentId].position, nodes[downIt->second].position))
                    {
                        mesh.connect(currentId, downIt->second);
                        connectionCount++;
                    }
                }
            }
        }
    }

    LOG("SimpleNavMeshBuilder: " << connectionCount << " connections created");

    return mesh;
}

bool SimpleNavMeshBuilder::hasLineOfSight(PhysicsSystem& physics, glm::vec3 a, glm::vec3 b) const
{
    glm::vec3 checkHeight(0.0f, 0.3f, 0.0f);
    glm::vec3 from = a + checkHeight;
    glm::vec3 to = b + checkHeight;

    glm::vec3 direction = to - from;
    float distance = glm::length(direction);
    if (distance < 0.001f) return true;

    direction /= distance;

    RaycastHit hit = physics.raycast(from, direction, distance);

    return !hit.hit || hit.distance > distance - 0.1f;
}
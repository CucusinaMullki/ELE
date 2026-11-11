#include "Nav/NavMesh.h"
#include <queue>
#include <unordered_set>
#include <limits>
#include <algorithm>

int NavMesh::addNode(glm::vec3 position)
{
    NavNode node;
    node.id = (int)m_nodes.size();
    node.position = position;
    m_nodes.push_back(node);
    return node.id;
}

void NavMesh::connect(int nodeIdA, int nodeIdB)
{
    if (nodeIdA < 0 || nodeIdA >= (int)m_nodes.size()) return;
    if (nodeIdB < 0 || nodeIdB >= (int)m_nodes.size()) return;
    if (nodeIdA == nodeIdB) return;

    auto& connA = m_nodes[nodeIdA].connections;
    if (std::find(connA.begin(), connA.end(), nodeIdB) == connA.end())
        connA.push_back(nodeIdB);

    auto& connB = m_nodes[nodeIdB].connections;
    if (std::find(connB.begin(), connB.end(), nodeIdA) == connB.end())
        connB.push_back(nodeIdA);
}

int NavMesh::getClosestNode(glm::vec3 position) const
{
    int closestId = -1;
    float closestDist2 = std::numeric_limits<float>::max();

    for (const auto& node : m_nodes)
    {
        float dist2 = glm::dot(node.position - position, node.position - position);
        if (dist2 < closestDist2)
        {
            closestDist2 = dist2;
            closestId = node.id;
        }
    }

    return closestId;
}

std::vector<glm::vec3> NavMesh::findPath(glm::vec3 startPos, glm::vec3 goalPos) const
{
    std::vector<glm::vec3> result;

    int startId = getClosestNode(startPos);
    int goalId = getClosestNode(goalPos);

    if (startId < 0 || goalId < 0) return result;
    if (startId == goalId)
    {
        result.push_back(m_nodes[goalId].position);
        return result;
    }

    auto heuristic = [this](int a, int b) {
        return glm::length(m_nodes[a].position - m_nodes[b].position);
    };

    using QueueEntry = std::pair<float, int>;
    auto cmp = [](const QueueEntry& a, const QueueEntry& b) { return a.first > b.first; };
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, decltype(cmp)> openSet(cmp);

    std::unordered_map<int, float> gScore;
    std::unordered_map<int, int> cameFrom;
    std::unordered_set<int> closedSet;

    gScore[startId] = 0.0f;
    openSet.push({ heuristic(startId, goalId), startId });

    bool found = false;

    while (!openSet.empty())
    {
        int current = openSet.top().second;
        openSet.pop();

        if (current == goalId)
        {
            found = true;
            break;
        }

        if (closedSet.count(current)) continue;
        closedSet.insert(current);

        for (int neighborId : m_nodes[current].connections)
        {
            if (closedSet.count(neighborId)) continue;

            float tentativeG = gScore[current] + glm::length(m_nodes[current].position - m_nodes[neighborId].position);

            auto it = gScore.find(neighborId);
            if (it == gScore.end() || tentativeG < it->second)
            {
                gScore[neighborId] = tentativeG;
                cameFrom[neighborId] = current;
                float f = tentativeG + heuristic(neighborId, goalId);
                openSet.push({ f, neighborId });
            }
        }
    }

    if (!found) return result;

    std::vector<int> pathIds;
    int current = goalId;
    while (current != startId)
    {
        pathIds.push_back(current);
        current = cameFrom[current];
    }
    pathIds.push_back(startId);
    std::reverse(pathIds.begin(), pathIds.end());

    result.reserve(pathIds.size());
    for (int id : pathIds)
        result.push_back(m_nodes[id].position);

    return result;
}

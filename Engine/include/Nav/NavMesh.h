#pragma once
#include "Nav/NavNode.h"
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

struct NavMeshChunk
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    std::vector<NavNode> nodes;
};

class NavMesh
{
public:
    int addNode(glm::vec3 position);
    void connect(int nodeIdA, int nodeIdB);

    int getClosestNode(glm::vec3 position) const;

    std::vector<glm::vec3> findPath(glm::vec3 startPos, glm::vec3 goalPos) const;

    const std::vector<NavNode>& getNodes() const {return m_nodes;}
    size_t getNodeCount() const { return m_nodes.size(); }
    void clear() { m_nodes.clear(); }

private:
    std::vector<NavNode> m_nodes;
};

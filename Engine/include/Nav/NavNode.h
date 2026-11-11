#pragma once
#include <glm/glm.hpp>
#include <vector>

struct NavNode
{
    int id = -1;
    glm::vec3 position{0.0f};
    std::vector<int> connections;
};

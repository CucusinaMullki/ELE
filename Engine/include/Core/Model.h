#pragma once
#include "Vulkan/Vertex.h"
#include <vector>
#include <string>

class Model
{
public:
    void load(const std::string& path);

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
};
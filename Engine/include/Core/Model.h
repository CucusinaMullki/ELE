#pragma once
#include "Vulkan/Vertex.h"
#include "Scene/Submesh.h"
#include <vector>
#include <string>

class Model
{
public:
    void load(const std::string& path);

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<Submesh>& getSubmesh() const { return m_submesh; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Submesh> m_submesh;
};

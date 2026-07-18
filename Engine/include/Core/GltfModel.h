#pragma once
#include "Vulkan/Vertex.h"
#include "Scene/Submesh.h"
#include <vector>
#include <string>

struct GltfImage
{
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
};

struct GltfMaterial
{
    int albedoImage = -1;
    int normalImage = -1;
    int mrImage = -1;
    int aoImage = -1;
};

class GltfModel
{
public:
    void load(const std::string& path);

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<Submesh>& getSubmesh() const { return m_submesh; }
    const std::vector<GltfImage>& getImage() const { return m_images; }
    const std::vector<GltfMaterial>& getMaterials() const { return m_materials; } 

private:
    void computeTangents();

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Submesh> m_submesh;
    std::vector<GltfImage> m_images;
    std::vector<GltfMaterial> m_materials;
    bool m_hasTangents = false;
};
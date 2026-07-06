#pragma once
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/IndexBuffer.h"
#include "Core/Model.h"
#include <string>

class Mesh
{
public:
    void load(vk::Device device, vk::PhysicalDevice physicalDevice, const std::string& modelPath);
    void cleanup();

    void bind(vk::CommandBuffer cmd) const;
    void draw(vk::CommandBuffer cmd) const;

private:
    VertexBuffer m_vertexBuffer;
    IndexBuffer m_indexBuffer;
};
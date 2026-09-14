#pragma once
#include "ASH/ASH.h"
#include "Vulkan/VoxelTexture3D.h"
#include <memory>
#include <glm/glm.hpp>

class Mesh;
class Material;

class DynamicVoxelProxy
{
public:
    void initialize(ASH::Device* device, uint32_t resolution, glm::vec3 localBoundsMin, glm::vec3 localBoundsSize);
    void cleanup();

    VoxelTexture3D& getLocalVoxelTexture() { return m_localVoxelTexture; }
    glm::vec3 getLocalBoundsMin() const { return m_localBoundsMin; }
    glm::vec3 getLocalBoundsSize() const { return m_localBoundsSize; }
    uint32_t getResolution() const { return m_resolution; }

    std::unique_ptr<ASH::DescriptorSet> stampDescriptorSet;

private:
    VoxelTexture3D m_localVoxelTexture;
    glm::vec3 m_localBoundsMin{};
    glm::vec3 m_localBoundsSize{};
    uint32_t m_resolution = 0;
};
#include "Vulkan/DynamicVoxelProxy.h"

void DynamicVoxelProxy::initialize(ASH::Device* device, uint32_t resolution, glm::vec3 localBoundsMin, glm::vec3 localBoundsSize)
{
    m_resolution = resolution;
    m_localBoundsMin = localBoundsMin;
    m_localBoundsSize = localBoundsSize;

    m_localVoxelTexture.initialize(device, resolution, ASH::Format::R8G8B8A8_UNorm);
}

void DynamicVoxelProxy::cleanup()
{
    m_localVoxelTexture.cleanup();
}
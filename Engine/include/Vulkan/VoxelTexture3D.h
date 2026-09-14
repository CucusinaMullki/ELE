#pragma once
#include "ASH/ASH.h"
#include <memory>

class VoxelTexture3D
{
public:
    void initialize(ASH::Device* device, uint32_t resolution, ASH::Format format, uint32_t mipLevels = 1);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    uint32_t getResolution() const { return m_resolution; }
    uint32_t getMipLevels() const { return m_mipLevels; }

private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    uint32_t m_resolution = 0;
    uint32_t m_mipLevels = 1;
};
#pragma once
#include "ASH/ASH.h"
#include <memory>

class VoxelTexture3D
{
public:
    void initialize(ASH::Device* device, uint32_t resolution, ASH::Format format);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    uint32_t getResolution() const { return m_resolution; }

private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    uint32_t m_resolution = 0;
};
#pragma once
#include "ASH/ASH.h"
#include <memory>

class DepthBuffer
{
public:
    void initialize(ASH::Device* device, ASH::Extent2D extent);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getFormat() const { return m_format; }

private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_format = ASH::Format::D32_Float;
};

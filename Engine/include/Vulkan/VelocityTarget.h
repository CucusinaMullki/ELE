#pragma once
#include "ASH/ASH.h"
#include <memory>

class VelocityTarget
{
public:
    void initialize(ASH::Device* device, ASH::Extent2D extent);
    void cleanup();

    ASH::Texture* getColorView() const { return m_colorTexture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getColorFormat() const { return m_colorFormat; }

private:
    std::unique_ptr<ASH::Texture> m_colorTexture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_colorFormat = ASH::Format::R16G16_Float;
};
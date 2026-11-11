#pragma once
#include "ASH/ASH.h"
#include <memory>

class LdrTarget
{
public:
    void initialize(ASH::Device* device, ASH::Extent2D extent);
    void cleanup();

    ASH::Texture* getColorTexture() const { return m_colorTexture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getColorFormat() const { return m_colorFormat; }

private:
    std::unique_ptr<ASH::Texture> m_colorTexture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_colorFormat = ASH::Format::R8G8B8A8_UNorm;
};

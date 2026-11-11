#pragma once
#include "ASH/ASH.h"
#include <vector>
#include <memory>

class BloomChain
{
public:
    static const uint32_t MIP_COUNT = 6;

    void initialize(ASH::Device* device, ASH::Extent2D fullExtent);
    void cleanup();

    ASH::Texture* getMipTexture(uint32_t level) const { return m_mipTextures[level].get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getFormat() const { return m_format; }
    ASH::Extent2D getMipExtent(uint32_t level) const { return m_mipExtents[level]; }
    uint32_t getMipCount() const { return MIP_COUNT; }

private:
    std::vector<std::unique_ptr<ASH::Texture>> m_mipTextures;
    std::vector<ASH::Extent2D> m_mipExtents;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_format = ASH::Format::R16G16B16A16_Float;
};

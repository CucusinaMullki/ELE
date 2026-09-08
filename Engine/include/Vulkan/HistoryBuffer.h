#pragma once
#include "ASH/ASH.h"
#include <array>
#include <memory>

class HistoryBuffer
{
public:
    void initialize(ASH::Device* device, ASH::Extent2D extent);
    void cleanup();

    ASH::Texture* getTexture(uint32_t index) const { return m_textures[index].get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getFormat() const { return m_format; }

private:
    std::array<std::unique_ptr<ASH::Texture>, 2> m_textures;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_format = ASH::Format::R16G16B16A16_Float;
};
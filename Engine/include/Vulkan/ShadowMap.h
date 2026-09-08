#pragma once
#include "ASH/ASH.h"
#include <memory>

class ShadowMap
{
public:
    void initialize(ASH::Device* device, uint32_t size);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getFormat() const { return m_format; }
    uint32_t getSize() const { return m_size; }
    
private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_format = ASH::Format::D32_Float;

    uint32_t m_size = 2048;
};
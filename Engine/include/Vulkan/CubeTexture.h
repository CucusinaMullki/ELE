#pragma once
#include "ASH/ASH.h"
#include <memory>

class CubeTexture
{
public:
    void initialize(ASH::Device* device, uint32_t size, ASH::Format format, uint32_t mipLevels = 1);
    void clear(ASH::Device* device, float r, float g, float b, float a);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    uint32_t getSize() const { return m_size; }
    uint32_t getMipLevels() const { return m_mipLevels; }

private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    uint32_t m_size = 0;
    uint32_t m_mipLevels = 1;
};
#pragma once
#include "ASH/ASH.h"
#include <memory>
#include <string>

class HdrTexture
{
public:
    void initialize(ASH::Device* device, const std::string& path);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

#pragma once
#include "ASH/ASH.h"
#include <memory>
#include <string>

class Texture
{
public:
    void initialize(ASH::Device* device, const std::string& path, bool srgb = true);
    void initializeFromMemory(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb = true);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    
private:
    void  upload(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb);

    std::unique_ptr<ASH::Sampler> m_sampler;
    std::unique_ptr<ASH::Texture> m_texture;
};
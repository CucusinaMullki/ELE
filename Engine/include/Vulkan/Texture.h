#pragma once
#include "ASH/ASH.h"
#include <memory>
#include <vector>
#include <string>

class Texture
{
public:
    void initialize(ASH::Device* device, const std::string& path, bool srgb = true);
    void initializeFromMemory(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb = true);
    void cleanup();
    struct MipInfo { uint64_t offset; uint32_t width; uint32_t height; };

    void initializeCompressed(ASH::Device* device, ASH::Format format,
        const std::vector<MipInfo>& mips,
        const void* data, size_t dataSize);
    bool initializeKtx2(ASH::Device* device, const std::string& path);

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    
private:
    void  upload(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb);

    std::unique_ptr<ASH::Sampler> m_sampler;
    std::unique_ptr<ASH::Texture> m_texture;
};
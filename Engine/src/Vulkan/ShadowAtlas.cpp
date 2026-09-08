#include "Vulkan/ShadowAtlas.h"

void ShadowAtlas::initialize(ASH::Device* device)
{
    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = m_format;
    texDesc.extent = { getWidth(), getHeight(), 1 };
    texDesc.usage = ASH::TextureUsage::DepthStencilAttachment | ASH::TextureUsage::Sampled;

    m_texture = device->createTexture(texDesc);

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Linear;
    samplerDesc.minFilter = ASH::Filter::Linear;
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.compareEnable = false;

    m_sampler = device->createSampler(samplerDesc);
}

void ShadowAtlas::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}
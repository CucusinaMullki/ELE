#include "Vulkan/ShadowMap.h"

void ShadowMap::initialize(ASH::Device* device, uint32_t size)
{
    m_size = size;

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = m_format;
    texDesc.extent = { size, size, 1 };
    texDesc.usage = ASH::TextureUsage::DepthStencilAttachment | ASH::TextureUsage::Sampled;

    m_texture = device->createTexture(texDesc);

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Linear;
    samplerDesc.minFilter = ASH::Filter::Linear;
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.compareEnable = true;
    samplerDesc.compareOp = ASH::CompareOp::Less;
    samplerDesc.minLod = 0.0f;
    samplerDesc.maxLod = 1.0f;

    m_sampler = device->createSampler(samplerDesc);
}

void ShadowMap::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}

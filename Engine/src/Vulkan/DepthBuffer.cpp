#include "Vulkan/DepthBuffer.h"

void DepthBuffer::initialize(ASH::Device* device, ASH::Extent2D extent)
{
    m_format = ASH::Format::D32_Float;

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = m_format;
    texDesc.extent = { extent.width, extent.height, 1 };
    texDesc.usage = ASH::TextureUsage::DepthStencilAttachment | ASH::TextureUsage::Sampled;

    m_texture = device->createTexture(texDesc);

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Nearest;
    samplerDesc.minFilter = ASH::Filter::Nearest;
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;

    m_sampler = device->createSampler(samplerDesc);
}

void DepthBuffer::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}
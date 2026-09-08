#include "Vulkan/VelocityTarget.h"

void VelocityTarget::initialize(ASH::Device* device, ASH::Extent2D extent)
{
    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = m_colorFormat;
    texDesc.extent = { extent.width, extent.height, 1 };
    texDesc.usage = ASH::TextureUsage::ColorAttachment | ASH::TextureUsage::Sampled;

    m_colorTexture = device->createTexture(texDesc);

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Nearest;
    samplerDesc.minFilter = ASH::Filter::Nearest;
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.compareEnable = false;

    m_sampler = device->createSampler(samplerDesc);
}

void VelocityTarget::cleanup()
{
    m_sampler.reset();
    m_colorTexture.reset();
}
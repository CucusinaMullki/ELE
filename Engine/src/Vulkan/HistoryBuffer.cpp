#include "Vulkan/HistoryBuffer.h"

void HistoryBuffer::initialize(ASH::Device* device, ASH::Extent2D extent)
{
    for (int i = 0; i < 2; i++)
    {
        ASH::TextureDesc texDesc{};
        texDesc.type = ASH::TextureType::Texture2D;
        texDesc.format = m_format;
        texDesc.extent = { extent.width, extent.height, 1 };
        texDesc.usage = ASH::TextureUsage::ColorAttachment | ASH::TextureUsage::Sampled;
        m_textures[i] = device->createTexture(texDesc);
    }

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Linear;
    samplerDesc.minFilter = ASH::Filter::Linear;
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.compareEnable = false;
    m_sampler = device->createSampler(samplerDesc);
}

void HistoryBuffer::cleanup()
{
    m_sampler.reset();
    for (auto& t : m_textures) t.reset();
}

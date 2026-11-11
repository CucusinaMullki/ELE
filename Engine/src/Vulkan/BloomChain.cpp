#include "Vulkan/BloomChain.h"
#include <algorithm>

void BloomChain::initialize(ASH::Device* device, ASH::Extent2D fullExtent)
{
    m_mipTextures.resize(MIP_COUNT);
    m_mipExtents.resize(MIP_COUNT);

    ASH::Extent2D currentExtent = fullExtent;

    for (uint32_t level = 0; level < MIP_COUNT; level++)
    {
        currentExtent.width = std::max(1u, currentExtent.width / 2);
        currentExtent.height = std::max(1u, currentExtent.height / 2);
        m_mipExtents[level] = currentExtent;

        ASH::TextureDesc texDesc{};
        texDesc.type = ASH::TextureType::Texture2D;
        texDesc.format = m_format;
        texDesc.extent = { currentExtent.width, currentExtent.height, 1 };
        texDesc.usage = ASH::TextureUsage::ColorAttachment | ASH::TextureUsage::Sampled;

        m_mipTextures[level] = device->createTexture(texDesc);
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

void BloomChain::cleanup()
{
    m_sampler.reset();
    m_mipTextures.clear();
    m_mipExtents.clear();
}

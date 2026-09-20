#include "Vulkan/Texture.h"
#include <stb_image.h>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include <algorithm>

void Texture::upload(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb)
{
    uint32_t mipLevels = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = srgb ? ASH::Format::R8G8B8A8_SRGB : ASH::Format::R8G8B8A8_UNorm;
    texDesc.extent = { (uint32_t)width, (uint32_t)height, 1 };
    texDesc.usage = ASH::TextureUsage::Sampled | ASH::TextureUsage::TransferDst | ASH::TextureUsage::TransferSrc;
    texDesc.mipLevels = mipLevels;

    m_texture = device->createTexture(texDesc);

    size_t dataSize = (size_t)width * (size_t)height * 4;

    ASH::BufferDesc stagingDesc{};
    stagingDesc.size = dataSize;
    stagingDesc.usage = ASH::BufferUsage::TransferSrc;
    stagingDesc.memory = ASH::MemoryUsage::CpuToGpu;

    auto stagingBuffer = device->createBuffer(stagingDesc);
    void* mapped = stagingBuffer->map();
    memcpy(mapped, pixels, dataSize);
    stagingBuffer->unmap();

    auto cmd = device->createCommandBuffer();
    cmd->begin();

    ASH::TextureBarrier wholeToTransferDst{};
    wholeToTransferDst.texture = m_texture.get();
    wholeToTransferDst.oldState = ASH::ResourceState::Undefined;
    wholeToTransferDst.newState = ASH::ResourceState::TransferDst;
    cmd->barrier(&wholeToTransferDst, 1, nullptr, 0);

    cmd->copyBufferToTexture(stagingBuffer.get(), m_texture.get());

    int32_t mipWidth = width;
    int32_t mipHeight = height;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
        ASH::TextureBarrier prevToSrc{};
        prevToSrc.texture = m_texture.get();
        prevToSrc.oldState = ASH::ResourceState::TransferDst;
        prevToSrc.newState = ASH::ResourceState::TransferSrc;
        prevToSrc.useSpecificMip = true;
        prevToSrc.mipLevel = i - 1;
        cmd->barrier(&prevToSrc, 1, nullptr, 0);

        int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

        ASH::TextureBlitRegion blitRegion{};
        blitRegion.srcTexture = m_texture.get();
        blitRegion.srcMipLevel = i - 1;
        blitRegion.srcExtentX = mipWidth;
        blitRegion.srcExtentY = mipHeight;
        blitRegion.dstTexture = m_texture.get();
        blitRegion.dstMipLevel = i;
        blitRegion.dstExtentX = nextWidth;
        blitRegion.dstExtentY = nextHeight;
        cmd->blitTexture(blitRegion);

        ASH::TextureBarrier prevToShaderRead{};
        prevToShaderRead.texture = m_texture.get();
        prevToShaderRead.oldState = ASH::ResourceState::TransferSrc;
        prevToShaderRead.newState = ASH::ResourceState::ShaderReadOnly;
        prevToShaderRead.useSpecificMip = true;
        prevToShaderRead.mipLevel = i - 1;
        cmd->barrier(&prevToShaderRead, 1, nullptr, 0);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    ASH::TextureBarrier lastToShaderRead{};
    lastToShaderRead.texture = m_texture.get();
    lastToShaderRead.oldState = ASH::ResourceState::TransferDst;
    lastToShaderRead.newState = ASH::ResourceState::ShaderReadOnly;
    lastToShaderRead.useSpecificMip = true;
    lastToShaderRead.mipLevel = mipLevels - 1;
    cmd->barrier(&lastToShaderRead, 1, nullptr, 0);

    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.maxLod = (float)mipLevels;
    m_sampler = device->createSampler(samplerDesc);
}

void Texture::initializeFromMemory(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb)
{
    upload(device, pixels, width, height, srgb);
}

void Texture::initialize(ASH::Device* device, const std::string& path, bool srgb)
{
    int width, height, channels;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load texture: " + path);

    upload(device, pixels, width, height, srgb);
    stbi_image_free(pixels);
}

void Texture::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}
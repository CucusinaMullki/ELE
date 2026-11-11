#include "Vulkan/Texture.h"
#include <stb_image.h>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <ktx.h>
#include <vulkan/vulkan_core.h>
#include <filesystem>
#include <cstdio>

static bool mapVkFormat(uint32_t vk, ASH::Format& out)
{
    switch (vk)
    {
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: out = ASH::Format::BC1_RGBA_UNorm; return true;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:  out = ASH::Format::BC1_RGBA_SRGB; return true;
        case VK_FORMAT_BC4_UNORM_BLOCK: out = ASH::Format::BC4_UNorm; return true;
        case VK_FORMAT_BC5_UNORM_BLOCK: out = ASH::Format::BC5_UNorm; return true;
        case VK_FORMAT_BC7_UNORM_BLOCK: out = ASH::Format::BC7_UNorm; return true;
        case VK_FORMAT_BC7_SRGB_BLOCK: out = ASH::Format::BC7_SRGB; return true;
        default: return false;
    }
}

bool Texture::initializeKtx2(ASH::Device* device, const std::string& path)
{
    ktxTexture2* kt = nullptr;
    if (ktxTexture2_CreateFromNamedFile(path.c_str(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kt) != KTX_SUCCESS)
        return false;

    auto fail = [&]() { ktxTexture_Destroy(ktxTexture(kt)); return false; };

    if (kt->numDimensions != 2 || kt->isCubemap || kt->isArray)
        return fail();

    if (ktxTexture2_NeedsTranscoding(kt))
        if (ktxTexture2_TranscodeBasis(kt, KTX_TTF_BC7_RGBA, 0) != KTX_SUCCESS)
            return fail();

    ASH::Format format;
    if (!mapVkFormat(kt->vkFormat, format))
        return fail();

    std::vector<MipInfo> mips(kt->numLevels);
    for (uint32_t level = 0; level < kt->numLevels; level++)
    {
        size_t offset = 0;
        if (ktxTexture_GetImageOffset(ktxTexture(kt), level, 0, 0, &offset) != KTX_SUCCESS)
            return fail();
        mips[level] = { (uint64_t)offset,
            std::max(1u, kt->baseWidth  >> level),
            std::max(1u, kt->baseHeight >> level) };
    }

    initializeCompressed(device, format, mips,
        ktxTexture_GetData(ktxTexture(kt)), ktxTexture_GetDataSize(ktxTexture(kt)));

    std::fprintf(stderr, "[KTX2] %s (%ux%u, %u mips, vkFormat=%u)\n",
        path.c_str(), kt->baseWidth, kt->baseHeight, kt->numLevels, kt->vkFormat);

    ktxTexture_Destroy(ktxTexture(kt));
    return true;
}

void Texture::initializeCompressed(ASH::Device* device, ASH::Format format, const std::vector<MipInfo>& mips, const void* data, size_t dataSize)
{
    if (mips.empty())
        throw std::runtime_error("compressed texture has no mip levels");

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = format;
    texDesc.extent = { mips[0].width, mips[0].height, 1 };
    texDesc.usage = ASH::TextureUsage::Sampled | ASH::TextureUsage::TransferDst;
    texDesc.mipLevels = (uint32_t)mips.size();
    m_texture = device->createTexture(texDesc);

    ASH::BufferDesc stagingDesc{};
    stagingDesc.size = dataSize;
    stagingDesc.usage = ASH::BufferUsage::TransferSrc;
    stagingDesc.memory = ASH::MemoryUsage::CpuToGpu;

    auto stagingBuffer = device->createBuffer(stagingDesc);
    void* mapped = stagingBuffer->map();
    memcpy(mapped, data, dataSize);
    stagingBuffer->unmap();

    std::vector<ASH::BufferTextureCopyRegion> regions(mips.size());
    for (size_t i = 0; i < mips.size(); i++)
    {
        regions[i].bufferOffset = mips[i].offset;
        regions[i].mipLevel = (uint32_t)i;
        regions[i].width = mips[i].width;
        regions[i].height = mips[i].height;
    }

    auto cmd = device->createCommandBuffer();
    cmd->begin();

    ASH::TextureBarrier toDst{};
    toDst.texture  = m_texture.get();
    toDst.oldState = ASH::ResourceState::Undefined;
    toDst.newState = ASH::ResourceState::TransferDst;
    cmd->barrier(&toDst, 1, nullptr, 0);

    cmd->copyBufferToTextureRegions(stagingBuffer.get(), m_texture.get(),
        regions.data(), (uint32_t)regions.size());

    ASH::TextureBarrier toRead{};
    toRead.texture  = m_texture.get();
    toRead.oldState = ASH::ResourceState::TransferDst;
    toRead.newState = ASH::ResourceState::ShaderReadOnly;
    cmd->barrier(&toRead, 1, nullptr, 0);

    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.maxLod = (float)mips.size();
    m_sampler = device->createSampler(samplerDesc);
}

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

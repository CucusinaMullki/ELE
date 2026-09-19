#include "Vulkan/Texture.h"
#include <stb_image.h>
#include <cstring>
#include <stdexcept>

void Texture::upload(ASH::Device* device, const unsigned char* pixels, int width, int height, bool srgb)
{
    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = srgb ? ASH::Format::R8G8B8A8_SRGB : ASH::Format::R8G8B8A8_UNorm;
    texDesc.extent = { (uint32_t)width, (uint32_t)height, 1 };
    texDesc.usage = ASH::TextureUsage::Sampled | ASH::TextureUsage::TransferDst;

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

    ASH::TextureBarrier toTransferDst{};
    toTransferDst.texture = m_texture.get();
    toTransferDst.oldState = ASH::ResourceState::Undefined;
    toTransferDst.newState = ASH::ResourceState::TransferDst;
    cmd->barrier(&toTransferDst, 1, nullptr, 0);

    cmd->copyBufferToTexture(stagingBuffer.get(), m_texture.get());

    ASH::TextureBarrier toShaderRead{};
    toShaderRead.texture = m_texture.get();
    toShaderRead.oldState = ASH::ResourceState::TransferDst;
    toShaderRead.newState = ASH::ResourceState::ShaderReadOnly;
    cmd->barrier(&toShaderRead, 1, nullptr, 0);

    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
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
#include "Vulkan/HdrTexture.h"
#include "Core/Path.h"
#include <stb_image.h>
#include <cstring>
#include <stdexcept>

void HdrTexture::initialize(ASH::Device* device, const std::string& path)
{
    std::string fullPath = (Paths::assetDir() / path).string();

    int width, height, channels;
    float* pixels = stbi_loadf(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load HDR texture: " + fullPath);

    m_width = (uint32_t)width;
    m_height = (uint32_t)height;

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture2D;
    texDesc.format = ASH::Format::R32G32B32A32_Float;
    texDesc.extent = { m_width, m_height, 1 };
    texDesc.usage = ASH::TextureUsage::Sampled | ASH::TextureUsage::TransferDst;

    m_texture = device->createTexture(texDesc);

    size_t dataSize = (size_t)m_width * (size_t)m_height * 4 * sizeof(float);

    ASH::BufferDesc stagingDesc{};
    stagingDesc.size = dataSize;
    stagingDesc.usage = ASH::BufferUsage::TransferSrc;
    stagingDesc.memory = ASH::MemoryUsage::CpuToGpu;

    auto stagingBuffer = device->createBuffer(stagingDesc);
    void* mapped = stagingBuffer->map();
    memcpy(mapped, pixels, dataSize);
    stagingBuffer->unmap();

    stbi_image_free(pixels);

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
    samplerDesc.magFilter = ASH::Filter::Linear;
    samplerDesc.minFilter = ASH::Filter::Linear;
    samplerDesc.addressModeU = ASH::AddressMode::Repeat;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    m_sampler = device->createSampler(samplerDesc);
}

void HdrTexture::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}
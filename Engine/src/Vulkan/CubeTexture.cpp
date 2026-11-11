#include "Vulkan/CubeTexture.h"
#include <cstring>

void CubeTexture::initialize(ASH::Device* device, uint32_t size, ASH::Format format, uint32_t mipLevels)
{
    m_size = size;
    m_mipLevels = mipLevels;

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::TextureCube;
    texDesc.format = format;
    texDesc.extent = { size, size, 1 };
    texDesc.arrayLayers = 6;
    texDesc.mipLevels = mipLevels;
    texDesc.usage = ASH::TextureUsage::Sampled | ASH::TextureUsage::ColorAttachment | ASH::TextureUsage::TransferDst;

    m_texture = device->createTexture(texDesc);

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Linear;
    samplerDesc.minFilter = ASH::Filter::Linear;
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;
    samplerDesc.minLod = 0.0f;
    samplerDesc.maxLod = (float)(mipLevels - 1);

    m_sampler = device->createSampler(samplerDesc);
}

void CubeTexture::clear(ASH::Device* device, float r, float g, float b, float a)
{
    float pixel[4] = { r, g, b, a };
    std::vector<float> data(4 * 6);
    for (int i = 0; i < 6; i++)
        memcpy(&data[i * 4], pixel, sizeof(pixel));

    size_t dataSize = data.size() * sizeof(float);

    ASH::BufferDesc stagingDesc{};
    stagingDesc.size = dataSize;
    stagingDesc.usage = ASH::BufferUsage::TransferSrc;
    stagingDesc.memory = ASH::MemoryUsage::CpuToGpu;
    auto stagingBuffer = device->createBuffer(stagingDesc);
    void* mapped = stagingBuffer->map();
    memcpy(mapped, data.data(), dataSize);
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
}

void CubeTexture::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}

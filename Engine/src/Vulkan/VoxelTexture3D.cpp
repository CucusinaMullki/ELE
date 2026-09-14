#include "Vulkan/VoxelTexture3D.h"

void VoxelTexture3D::initialize(ASH::Device* device, uint32_t resolution, ASH::Format format, uint32_t mipLevels)
{
    m_resolution = resolution;
    m_mipLevels = mipLevels;

    ASH::TextureDesc texDesc{};
    texDesc.type = ASH::TextureType::Texture3D;
    texDesc.format = format;
    texDesc.extent = { resolution, resolution, resolution };
    texDesc.mipLevels = mipLevels;
    texDesc.usage = ASH::TextureUsage::Storage | ASH::TextureUsage::Sampled | ASH::TextureUsage::TransferDst;

    m_texture = device->createTexture(texDesc);

    auto cmd = device->createCommandBuffer();
    cmd->begin();
    ASH::TextureBarrier tb{};
    tb.texture = m_texture.get();
    tb.oldState = ASH::ResourceState::Undefined;
    tb.newState = ASH::ResourceState::General;
    cmd->barrier(&tb, 1, nullptr, 0);
    cmd->end();

    ASH::SubmitInfo submitInfo{};
    submitInfo.commandBuffer = cmd.get();
    device->submit(submitInfo);
    device->waitIdle();

    ASH::SamplerDesc samplerDesc{};
    samplerDesc.magFilter = ASH::Filter::Linear;
    samplerDesc.minFilter = ASH::Filter::Linear;
    samplerDesc.minLod = 0.0f;
    samplerDesc.maxLod = (float)(mipLevels -1);
    samplerDesc.addressModeU = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = ASH::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = ASH::AddressMode::ClampToEdge;

    m_sampler = device->createSampler(samplerDesc);
}

void VoxelTexture3D::cleanup()
{
    m_sampler.reset();
    m_texture.reset();
}
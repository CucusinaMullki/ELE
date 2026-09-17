#include "Vulkan/Pipeline.h"
#include "Vulkan/SkinnedVertex.h"
#include "Vulkan/Vertex.h"
#include "Vulkan/ShaderLoader.h"
#include <glm/glm.hpp>
#include "Core/Log.h"
#include <array>

void Pipeline::initialize(ASH::Device* device, const PipelineConfig& config)
{
    m_device = device;

    ASH::DescriptorSetLayoutDesc layoutDesc = buildLayoutDesc(config.layoutType);
    m_descriptorSetLayout = device->createDescriptorSetLayout(layoutDesc);

    bool hasLayout = (config.layoutType != LayoutType::None);
    auto vertCode = loadSpirvFile(config.vertexShader + ".vert");

    ASH::GraphicsPipelineDesc desc{};

    ASH::ShaderStageDesc vertStage{};
    vertStage.stage = ASH::ShaderStage::Vertex;
    vertStage.spirvCode = vertCode.data();
    vertStage.spirvSize = vertCode.size() * sizeof(uint32_t);
    desc.stages.push_back(vertStage);

    std::vector<uint32_t> fragCode;
    if (!config.fragmentShader.empty())
    {
        fragCode = loadSpirvFile(config.fragmentShader + ".frag");
        ASH::ShaderStageDesc fragStage{};
        fragStage.stage = ASH::ShaderStage::Fragment;
        fragStage.spirvCode = fragCode.data();
        fragStage.spirvSize = fragCode.size() * sizeof(uint32_t);
        desc.stages.push_back(fragStage);
    }

    if (config.useVertexInput)
    {
        if (config.layoutType == LayoutType::Skinned)
        {
            auto binding = SkinnedVertex::getBindingDescriptorASH();
            auto arr = SkinnedVertex::getAttributeDescriptionsASH();
            desc.vertexInput.binding.push_back(binding);
            desc.vertexInput.attributes.assign(arr.begin(), arr.end());
        }
        else
        {
            auto binding = Vertex::getBindingDescriptorASH();
            auto arr = Vertex::getAttributeDescriptionsASH();
            desc.vertexInput.binding.push_back(binding);
            desc.vertexInput.attributes.assign(arr.begin(), arr.end());
        }
    }

    desc.topology = ASH::PrimitiveTopology::TriangleList;

    desc.rasterization.polygonMode = ASH::PolygonMode::Fill;
    desc.rasterization.cullMode = config.cullMode;
    desc.rasterization.frontFace = ASH::FrontFace::CounterClockwise;
    desc.rasterization.lineWidth = 1.0f;
    desc.rasterization.depthBiasEnable = config.depthBiasEnable;
    desc.rasterization.depthBiasConstantFactor = config.depthBiasConstant;
    desc.rasterization.depthBiasSlopeFactor = config.depthBiasSlope;

    desc.depthStencil.depthTestEnable = config.depthTest;
    desc.depthStencil.depthWriteEnable = config.depthWrite;
    desc.depthStencil.depthCompareOp = config.depthCompareOp;

    for (uint32_t i = 0; i < config.colorAttachmentCount; i++)
    {
        ASH::ColorBlendAttachmentState cba{};
        cba.blendEnable = config.blendEnable;
        cba.srcColorFactor = config.blendSrcFactor;
        cba.dstColorFactor = config.blendDstFactor;
        cba.colorBlendOp = config.blendOp;
        cba.srcAlphaFactor = ASH::BlendFactor::One;
        cba.dstAlphaFactor = ASH::BlendFactor::Zero;
        cba.alphaBlendOp = ASH::BlendOp::Add;
        desc.colorBlend.attachments.push_back(cba);
    }

    desc.renderPass = config.renderPass;

    if (m_descriptorSetLayout)
        desc.descriptorSetLayouts.push_back(m_descriptorSetLayout.get());

    ASH::PushConstantRange pcRange{};
    pcRange.stageFlags = config.pushStage;
    pcRange.offset = 0;
    pcRange.size = config.pushConstantSize;
    desc.pushConstantRanges.push_back(pcRange);

    m_pipeline = device->createGraphicsPipeline(desc);
}

ASH::DescriptorSetLayoutDesc buildLayoutDesc(LayoutType layoutType)
{

    ASH::DescriptorSetLayoutDesc layoutDesc{};

    auto addBinding = [&](uint32_t binding, ASH::DescriptorType type, ASH::ShaderStage stages) {
        ASH::DescriptorBindingDesc b{};
        b.binding = binding;
        b.type = type;
        b.count = 1;
        b.stageFlags = stages;
        layoutDesc.bindings.push_back(b);
    };

    switch (layoutType)
    {
    case LayoutType::Fog:
        addBinding(0, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(1, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(2, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(3, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        break;
    
    case LayoutType::Decal:
        addBinding(0, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(1, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(2, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment);
        addBinding(3, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(4, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(5, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(6, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::Skinned:
        addBinding(0, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment);
        addBinding(1, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(2, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(3, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(4, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(5, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(6, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(7, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(8, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(9, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Vertex);
        addBinding(10, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::PostProcess:
        for (uint32_t b = 0; b < 3; b++)
            addBinding(b, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::Default:
        addBinding(0, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Vertex | ASH::ShaderStage::Fragment);
        addBinding(1, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(2, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(3, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(4, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(5, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(6, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(7, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(8, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(9, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(10, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(11, ASH::DescriptorType::StorageBuffer, ASH::ShaderStage::Fragment);
        addBinding(12, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::SingleTexture:
        addBinding(0, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::Skybox:
        addBinding(0, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Vertex);
        addBinding(1, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::VoxelClear:
        addBinding(0, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Compute);
        break;

    case LayoutType::Voxelize:
        addBinding(0, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(1, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Fragment);
        addBinding(2, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(3, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(4, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(5, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        break;

    case LayoutType::StampProxy:
        addBinding(0, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Compute);
        addBinding(1, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Compute);
        break;

    case LayoutType::VoxelizeSkinned:
        addBinding(0, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(1, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Fragment);
        addBinding(2, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(3, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(4, ASH::DescriptorType::CombinedImageSampler, ASH::ShaderStage::Fragment);
        addBinding(5, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Fragment);
        addBinding(6, ASH::DescriptorType::UniformBuffer, ASH::ShaderStage::Vertex);
        break;
    case LayoutType::DownsampleVoxels:
        addBinding(0, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Compute);
        addBinding(1, ASH::DescriptorType::StorageImage, ASH::ShaderStage::Compute);
        break;

    case LayoutType::None:
        break;
    }

    return layoutDesc;
}

void Pipeline::cleanup()
{
    m_pipeline.reset();
    m_descriptorSetLayout.reset();
}
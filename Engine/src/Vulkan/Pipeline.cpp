#include "Vulkan/Pipeline.h"
#include "Vulkan/Shader.h"
#include "Vulkan/SkinnedVertex.h"
#include "Vulkan/Vertex.h"
#include "Vulkan/ShaderLoader.h"
#include <glm/glm.hpp>
#include "Core/Log.h"
#include <array>

void Pipeline::initialize(ASH::Device* device, const PipelineConfig& config)
{
    m_device = device;

    ASH::DescriptorSetLayoutDesc layoutDesc{};

    auto addBinding = [&](uint32_t binding, ASH::DescriptorType type, ASH::ShaderStage stages) {
        ASH::DescriptorBindingDesc b{};
        b.binding = binding;
        b.type = type;
        b.count = 1;
        b.stageFlags = stages;
        layoutDesc.bindings.push_back(b);
    };

    switch (config.layoutType)
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
        break;

    case LayoutType::None:
        break;
    }

    bool hasLayout = (config.layoutType != LayoutType::None);

    if (hasLayout)
        m_descriptorSetLayout = device->createDescriptorSetLayout(layoutDesc);

    auto vertCode = loadSpirvFile(config.vertexShader);

    ASH::GraphicsPipelineDesc desc{};

    ASH::ShaderStageDesc vertStage{};
    vertStage.stage = ASH::ShaderStage::Vertex;
    vertStage.spirvCode = vertCode.data();
    vertStage.spirvSize = vertCode.size() * sizeof(uint32_t);

    std::vector<uint32_t> fragCode;
    if (!config.fragmentShader.empty())
    {
        fragCode = loadSpirvFile(config.fragmentShader);
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
}

void Pipeline::cleanup()
{
    m_descriptorSetLayout.reset();
    m_pipeline.reset();
}
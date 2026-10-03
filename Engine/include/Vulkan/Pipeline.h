#pragma once
#include "ASH/ASH.h"
#include <string>
#include <vector>
#include <memory>

enum class LayoutType
{
    None,
    Default,
    Fog,
    Decal,
    Skinned,
    PostProcess,
    SingleTexture,
    Skybox,
    VoxelClear,
    Voxelize,
    StampProxy,
    VoxelizeSkinned,
    DownsampleVoxels,
    SkinnedShadow,
};

struct PipelineConfig
{
    std::string vertexShader;
    std::string fragmentShader = "";
    ASH::RenderPass* renderPass = nullptr;
    uint32_t frameCount = 1;
    uint32_t maxMaterials = 100;
    uint32_t pushConstantSize = sizeof(float) * 32;
    ASH::BlendFactor blendSrcFactor = ASH::BlendFactor::One;
    ASH::BlendFactor blendDstFactor = ASH::BlendFactor::Zero;
    ASH::BlendOp blendOp = ASH::BlendOp::Add;
    ASH::CompareOp depthCompareOp = ASH::CompareOp::Less;
    ASH::ShaderStage pushStage = ASH::ShaderStage::Vertex;
    ASH::CullMode cullMode = ASH::CullMode::Back;
    bool depthClampEnable = false;
    uint32_t colorAttachmentCount = 1;
    bool depthWrite = true;
    bool depthTest = true;
    bool blendEnable = false;
    bool useVertexInput = true;
    bool depthBiasEnable = false;
    bool skinnedLayout = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    LayoutType layoutType = LayoutType::Default;
};

ASH::DescriptorSetLayoutDesc buildLayoutDesc(LayoutType layoutType);

class Pipeline
{
public:
    void initialize(ASH::Device* device, const PipelineConfig& config);
    void cleanup();

    ASH::DescriptorSetLayout* getDescriptorSetLayout() const { return m_descriptorSetLayout.get(); }
    std::unique_ptr<ASH::DescriptorSet> createDescriptorSet() { return m_device->createDescriptorSet(m_descriptorSetLayout.get()); }
    ASH::Pipeline* getPipeline() const { return m_pipeline.get(); }

private:
    ASH::Device* m_device = nullptr;
    std::unique_ptr<ASH::DescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<ASH::Pipeline> m_pipeline;
};
#pragma once
#include "vulkan/vulkan.hpp"
#include <string>
#include <vector>

class Shader
{
public:
    void load(vk::Device device, const std::string& name, vk::ShaderStageFlagBits stage);
    void cleanup();

    vk::PipelineShaderStageCreateInfo getStageInfo() const;

private:
    std::vector<char> readFile(const std::string& path);
    std::string resolvePath(const std::string& name, vk::ShaderStageFlagBits stage);

    vk::UniqueShaderModule m_module;
    vk::ShaderStageFlagBits m_stage;
};
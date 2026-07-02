#include "Vulkan/Shader.h"
#include "Core/Path.h"
#include <fstream>
#include <stdexcept>

void Shader::load(vk::Device device, const std::string& name, vk::ShaderStageFlagBits stage)
{
    m_stage = stage;
    auto code = readFile(resolvePath(name, stage));

    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.setCodeSize(code.size());
    createInfo.setPCode(reinterpret_cast<const uint32_t*>(code.data()));

    m_module = device.createShaderModuleUnique(createInfo);
}

void Shader::cleanup()
{
    m_module.reset();
}

vk::PipelineShaderStageCreateInfo Shader::getStageInfo() const
{
    vk::PipelineShaderStageCreateInfo info{};
    info.setStage(m_stage);
    info.setModule(m_module.get());
    info.setPName("main");
    return info;
}

std::string Shader::resolvePath(const std::string& name, vk::ShaderStageFlagBits stage)
{
    std::string ext;
    switch (stage)
    {
        case vk::ShaderStageFlagBits::eVertex:   ext = ".vert.spv"; break;
        case vk::ShaderStageFlagBits::eFragment: ext = ".frag.spv"; break;
        case vk::ShaderStageFlagBits::eCompute:  ext = ".comp.spv"; break;
        default: throw std::runtime_error("unsupported shader stage");
    }

    auto path = Paths::assetDir() / "shaders" / (name + ext);
    return path.string();
}

std::vector<char> Shader::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open shader: " + path);

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}
#include "Vulkan/ShaderLoader.h"
#include "Core/Path.h"
#include <fstream>
#include <stdexcept>

std::vector<uint32_t> loadSpirvFile(const std::string& name)
{
    auto path = Paths::assetDir() / "shaders" / (name + ".spv");

    std::ifstream file(path.string(), std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open SPIR-V file: " + path.string());

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

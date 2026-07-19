#include "Vulkan/PipelineCache.h"
#include "Core/Path.h"
#include "Core/Log.h"
#include <fstream>
#include <vector>

void PipelineCache::initialize(vk::Device device)
{
    m_device = device;
    m_cachePath = (Paths::executableDir() / "pipeline_cache.bin").string();

    std::vector<char> cacheData;
    std::ifstream file(m_cachePath, std::ios::binary | std::ios::ate);
    if (file.is_open())
    {
        size_t size = static_cast<size_t>(file.tellg());
        file.seekg(0);
        cacheData.resize(size);
        file.read(cacheData.data(), size);
        file.close();
        LOG("Pipeline cache loaded: " << size << " bytes");
    }
    else
    {
        LOG("No pipeline cache found - creating new");
    }

    vk::PipelineCacheCreateInfo cacheInfo{};
    if (!cacheData.empty())
    {
        cacheInfo.setInitialDataSize(cacheData.size());
        cacheInfo.setPInitialData(cacheData.data());
    }

    m_cache = m_device.createPipelineCacheUnique(cacheInfo);
}

void PipelineCache::save()
{
    if (!m_cache) return;

    auto data = m_device.getPipelineCacheData(m_cache.get());

    std::ofstream file(m_cachePath, std::ios::binary);
    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        LOG("Pipeline cache saved: " << data.size() << " bytes");
    }
}

void PipelineCache::cleanup()
{
    m_cache.reset();
}
#pragma once
#include <vulkan/vulkan.hpp>
#include <string>

class PipelineCache
{
public:
    void initialize(vk::Device device);
    void save();
    void cleanup();

    vk::PipelineCache get() const { return m_cache.get(); }
private:
    vk::Device m_device;
    vk::UniquePipelineCache m_cache;
    std::string m_cachePath;
};
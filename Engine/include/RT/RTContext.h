#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <ASH/Device.h>
#include <ASH/Buffer.h>
#include <functional>
#include <memory>

namespace RT
{
class Context
{
public:
    bool init(ASH::Device* device);
    void shutdown() { m_device = nullptr; m_dev = VK_NULL_HANDLE; }

    ASH::Device* ash() const { return m_device; }
    VkDevice device() const { return m_dev; }
    VkPhysicalDevice physical() const { return m_phys; }
    VkQueue queue() const { return m_queue; }
    uint32_t family() const { return m_family; }
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProps() const { return m_rtProps; }
    const VkPhysicalDeviceAccelerationStructurePropertiesKHR& asProps() const { return m_asProps; }
    bool submitOneTime(const std::function<void(VkCommandBuffer)>& record);

    std::unique_ptr<ASH::Buffer> createBuffer(size_t size, ASH::BufferUsage usage,
        ASH::MemoryUsage mem, const char* name) const
    {
        ASH::BufferDesc d; d.size = size; d.usage = usage; d.memory = mem; d.debugName = name;
        return m_device->createBuffer(d);
    }

    PFN_vkCreateAccelerationStructureKHR vkCreateAS = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAS = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetASSizes = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAS = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetASAddr = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRTPipelines = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetGroupHandles = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRays = nullptr;
private:
    ASH::Device* m_device = nullptr;
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_dev = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_family = 0;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProps{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProps{};
};
}

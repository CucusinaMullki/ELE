#include "RT/RTContext.h"

namespace RT {

bool Context::init(ASH::Device* device)
{
    if (!device || !device->isRayTracingSupported()) return false;

    const ASH::NativeDeviceHandles h = device->getNativeHandles();
    m_device = device;
    m_phys   = reinterpret_cast<VkPhysicalDevice>(h.physicalDevice);
    m_dev    = reinterpret_cast<VkDevice>(h.device);
    m_queue  = reinterpret_cast<VkQueue>(h.graphicsQueue);
    m_family = h.graphicsFamily;

#define LOAD(member, name) \
    member = (PFN_##name)vkGetDeviceProcAddr(m_dev, #name); if (!member) return false;
    LOAD(vkCreateAS, vkCreateAccelerationStructureKHR)
    LOAD(vkDestroyAS, vkDestroyAccelerationStructureKHR)
    LOAD(vkGetASSizes, vkGetAccelerationStructureBuildSizesKHR)
    LOAD(vkCmdBuildAS, vkCmdBuildAccelerationStructuresKHR)
    LOAD(vkGetASAddr, vkGetAccelerationStructureDeviceAddressKHR)
    LOAD(vkCreateRTPipelines, vkCreateRayTracingPipelinesKHR)
    LOAD(vkGetGroupHandles, vkGetRayTracingShaderGroupHandlesKHR)
    LOAD(vkCmdTraceRays, vkCmdTraceRaysKHR)
#undef LOAD

    m_rtProps = {};
    m_rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    m_asProps = {};
    m_asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    m_rtProps.pNext = &m_asProps;
    VkPhysicalDeviceProperties2 p2{};
    p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &m_rtProps;
    vkGetPhysicalDeviceProperties2(m_phys, &p2);
    m_rtProps.pNext = nullptr;
    return true;
}

bool Context::submitOneTime(const std::function<void(VkCommandBuffer)>& record)
{
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pi.queueFamilyIndex = m_family;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(m_dev, &pi, nullptr, &pool) != VK_SUCCESS) return false;

    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    bool ok = vkAllocateCommandBuffers(m_dev, &ai, &cmd) == VK_SUCCESS;

    VkFence fence = VK_NULL_HANDLE;
    if (ok)
    {
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);
        record(cmd);
        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkCreateFence(m_dev, &fi, nullptr, &fence);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        ok = vkQueueSubmit(m_queue, 1, &si, fence) == VK_SUCCESS &&
            vkWaitForFences(m_dev, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    }
    if (fence) vkDestroyFence(m_dev, fence, nullptr);
    vkDestroyCommandPool(m_dev, pool, nullptr);
    return ok;
}

}
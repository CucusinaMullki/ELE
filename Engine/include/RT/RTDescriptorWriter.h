#pragma once
#include <vulkan/vulkan.h>
#include <ASH/Texture.h>
#include <ASH/Buffer.h>
#include <deque>
#include <vector>

namespace RT
{

class DescriptorWriter
{
public:
    explicit DescriptorWriter(VkDescriptorSet set) : m_set(set) {}

    DescriptorWriter& image(uint32_t binding, VkDescriptorType type, ASH::Texture* tex,
        VkImageLayout layout, VkSampler sampler = VK_NULL_HANDLE)
    {
        if (!tex) return *this;
        m_images.push_back({ sampler, reinterpret_cast<VkImageView>(tex->getFaceView(0, 0)), layout });
        VkWriteDescriptorSet w = base(binding, type);
        w.pImageInfo = &m_images.back();
        m_writes.push_back(w);
        return *this;
    }

    DescriptorWriter& buffer(uint32_t binding, VkDescriptorType type, VkBuffer buf)
    {
        if (!buf) return *this;
        m_buffers.push_back({ buf, 0, VK_WHOLE_SIZE });
        VkWriteDescriptorSet w = base(binding, type);
        w.pBufferInfo = &m_buffers.back();
        m_writes.push_back(w);
        return *this;
    }

    DescriptorWriter& buffer(uint32_t binding, VkDescriptorType type, ASH::Buffer* buf)
    {
        return buffer(binding, type, buf ? reinterpret_cast<VkBuffer>(buf->getNativeHandle()) : VK_NULL_HANDLE);
    }

    DescriptorWriter& accelerationStructure(uint32_t binding, VkAccelerationStructureKHR as)
    {
        if (!as) return *this;
        m_handles.push_back(as);
        VkWriteDescriptorSetAccelerationStructureKHR asw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        asw.accelerationStructureCount = 1;
        asw.pAccelerationStructures = &m_handles.back();
        m_asInfos.push_back(asw);
        VkWriteDescriptorSet w = base(binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
        w.pNext = &m_asInfos.back();
        m_writes.push_back(w);
        return *this;
    }

    void commit(VkDevice dev)
    {
        if (!m_writes.empty())
            vkUpdateDescriptorSets(dev, uint32_t(m_writes.size()), m_writes.data(), 0, nullptr);
    }

private:
    static VkBuffer native(ASH::Buffer* b) { return reinterpret_cast<VkBuffer>(b->getNativeHandle()); }

    VkWriteDescriptorSet base(uint32_t binding, VkDescriptorType type) const
    {
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = m_set;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = type;
        return w;
    }

    VkDescriptorSet m_set;
    std::vector<VkWriteDescriptorSet> m_writes;
    std::deque<VkDescriptorImageInfo> m_images;
    std::deque<VkDescriptorBufferInfo> m_buffers;
    std::deque<VkAccelerationStructureKHR> m_handles;
    std::deque<VkWriteDescriptorSetAccelerationStructureKHR> m_asInfos;
};
}
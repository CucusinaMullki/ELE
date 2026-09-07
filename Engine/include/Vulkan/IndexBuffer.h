#pragma once
#include "ASH/ASH.h"
#include <memory>
#include <cstdint>
#include <vector>

class IndexBuffer
{
public:
    void initialize(ASH::Device* device, const std::vector<uint32_t>& indices);

    void cleanup();

    ASH::Buffer* getBuffer() const { return m_buffer.get(); }
    uint32_t getIndexCount() const { return m_indexCount; }

private:
    uint32_t m_indexCount = 0;
    std::unique_ptr<ASH::Buffer> m_buffer;
};
#include "Scene/PropertyIO.h"

void BinaryPropertyWriter::begin()
{
    m_count = 0;
    m_countAt = m_w.size();
    m_w.write<uint32_t>(0);
}

void BinaryPropertyWriter::end()
{
    m_w.patch<uint32_t>(m_countAt, m_count);
}

void BinaryPropertyWriter::header(const char* name, PropType t)
{
    m_w.write<uint32_t>(propHash(name));
    m_w.write<uint8_t>((uint8_t)t);
    ++m_count;
}

void BinaryPropertyWriter::field(const char* n, std::string& v)
{
    header(n, PropType::String);
    m_w.writeString(v);
}

namespace
{
    uint32_t fixedSize(PropType t)
    {
        switch (t)
        {
            case PropType::Float: return 4;
            case PropType::Int: return 4;
            case PropType::Enum: return 4;
            case PropType::Bool: return 1;
            case PropType::Vec2: return 8;
            case PropType::Vec3: return 12;
            case PropType::Vec4: return 16;
            case PropType::Quat: return 16;
            default: return 0;
        }
    }
}

BinaryPropertyReader::BinaryPropertyReader(BinaryReader r)
{
    const uint32_t count = r.read<uint32_t>();
    for (uint32_t i = 0; i < count && r.ok(); ++i)
    {
        const uint32_t hash = r.read<uint32_t>();
        const auto type = (PropType)r.read<uint8_t>();

        Entry e{ type, nullptr, 0 };
        if (type == PropType::String)
        {
            e.size = r.read<uint32_t>();
            e.data = r.cursor();
            r.skip(e.size);
        }
        else
        {
            e.size = fixedSize(type);
            if (e.size == 0) { m_ok = false; break; }
            e.data = r.cursor();
            r.skip(e.size);
        }
        if (r.ok()) m_fields[hash] = e;
    }
    m_ok = m_ok && r.ok();
}

const BinaryPropertyReader::Entry* BinaryPropertyReader::find(const char* name, PropType t) const
{
    auto it = m_fields.find(propHash(name));
    if (it == m_fields.end() || it->second.type != t) return nullptr;
    return &it->second;
}

void BinaryPropertyReader::field(const char* n, bool& v)
{
    if (const Entry* e = find(n, PropType::Bool)) v = e->data[0] != 0;
}

void BinaryPropertyReader::field(const char* n, std::string& v)
{
    if (const Entry* e = find(n, PropType::String))
        v.assign(reinterpret_cast<const char*>(e->data), e->size);
}

void BinaryPropertyReader::enumField(const char* n, int32_t& v, std::span<const char* const> names)
{
    const Entry* e = find(n, PropType::Enum);
    if (!e) e = find(n, PropType::Int);
    if (!e) return;
    int32_t x;
    std::memcpy(&x, e->data, 4);
    if (names.empty() || (x >= 0 && x < (int32_t)names.size())) v = x;
}

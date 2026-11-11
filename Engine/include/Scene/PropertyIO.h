#pragma once
#include "Scene/Property.h"
#include "Core/BinaryStream.h"
#include <unordered_map>

class BinaryPropertyWriter final : public PropertyVisitor
{
public:
    explicit BinaryPropertyWriter(BinaryWriter& w) : m_w(w) {}

    void begin();
    void end();

    void field(const char* n, float& v) override { put(n, PropType::Float, v); }
    void field(const char* n, int32_t& v) override { put(n, PropType::Int, v); }
    void field(const char* n, bool& v) override { uint8_t b = v ? 1 : 0; put(n, PropType::Bool, b); }
    void field(const char* n, glm::vec2& v) override { put(n, PropType::Vec2, v); }
    void field(const char* n, glm::vec3& v) override { put(n, PropType::Vec3, v); }
    void field(const char* n, glm::vec4& v) override { put(n, PropType::Vec4, v); }
    void field(const char* n, glm::quat& v) override { put(n, PropType::Quat, v); }
    void field(const char* n, std::string& v) override;
    void enumField(const char* n, int32_t& v, std::span<const char* const>) override { put(n, PropType::Enum, v); }

private:
    template<typename T>
    void put(const char* name, PropType t, const T& v)
    {
        header(name, t);
        m_w.write(v);
    }
    void header(const char* name, PropType t);

    BinaryWriter& m_w;
    size_t m_countAt = 0;
    uint32_t m_count = 0;
};

class BinaryPropertyReader final : public PropertyVisitor
{
public:
    explicit BinaryPropertyReader(BinaryReader r);

    bool ok() const { return m_ok; }

    void field(const char* n, float& v) override { get(n, PropType::Float, v); }
    void field(const char* n, int32_t& v) override { get(n, PropType::Int, v); }
    void field(const char* n, bool& v) override;
    void field(const char* n, glm::vec2& v) override { get(n, PropType::Vec2, v); }
    void field(const char* n, glm::vec3& v) override { get(n, PropType::Vec3, v); }
    void field(const char* n, glm::vec4& v) override { get(n, PropType::Vec4, v); }
    void field(const char* n, glm::quat& v) override { get(n, PropType::Quat, v); }
    void field(const char* n, std::string& v) override;
    void enumField(const char* n, int32_t& v, std::span<const char* const> names) override;

private:
    struct Entry { PropType type; const uint8_t* data; uint32_t size; };

    const Entry* find(const char* name, PropType t) const;

    template<typename T>
    void get(const char* name, PropType t, T& v)
    {
        if (const Entry* e = find(name, t); e && e->size == sizeof(T))
            std::memcpy(&v, e->data, sizeof(T));
    }

    std::unordered_map<uint32_t, Entry> m_fields;
    bool m_ok = true;
};

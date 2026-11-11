#pragma once
#include <cstdint>
#include <string>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

constexpr uint32_t propHash(const char* s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

enum class PropType : uint8_t
{
    Float = 1, Int, Bool, Vec2, Vec3, Vec4, Quat, String, Enum
};

struct PropertyVisitor
{
    virtual ~PropertyVisitor() = default;

    virtual void field(const char* name, float& v) = 0;
    virtual void field(const char* name, int32_t& v) = 0;
    virtual void field(const char* name, bool& v) = 0;
    virtual void field(const char* name, glm::vec2& v) = 0;
    virtual void field(const char* name, glm::vec3& v) = 0;
    virtual void field(const char* name, glm::vec4& v) = 0;
    virtual void field(const char* name, glm::quat& v) = 0;
    virtual void field(const char* name, std::string& v) = 0;

    virtual void enumField(const char* name, int32_t& v, std::span<const char* const> names) = 0;

    template<typename E>
    void enumOf(const char* name, E& e, std::span<const char* const> names)
    {
        int32_t v = (int32_t)e;
        enumField(name, v, names);
        e = (E)v;
    }
};

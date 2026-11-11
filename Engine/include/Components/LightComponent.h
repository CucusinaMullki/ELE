#pragma once
#include "Scene/IComponent.h"
#include "Scene/Transform.h"
#include "Scene/Light.h"
#include <algorithm>
#include <cmath>

class LightComponent : public IComponent
{
public:
    LightType type = LightType::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 0.0f;
    float innerAngle = 20.0f;
    float outerAngle = 30.0f;
    glm::vec2 size{1.0f, 1.0f};
    bool twoSided = false;
    bool castShadows = true;
    bool enabled = true;

    const char* typeName() const override { return "Light"; }

    void visit(PropertyVisitor& v) override
    {
        static const char* const kTypes[] = { "Directional", "Point", "Spot", "Rect" };
        v.enumOf("type", type, kTypes);
        v.field("color", color);
        v.field("intensity", intensity);
        v.field("range", range);
        v.field("innerAngle", innerAngle);
        v.field("outerAngle", outerAngle);
        v.field("size", size);
        v.field("twoSided", twoSided);
        v.field("castShadows", castShadows);
        v.field("enable", enabled);
    }

    Light toGpu(const Transform& t) const
    {
        const glm::mat4 m = t.getMatrix();
        const glm::vec3 fwd = -glm::normalize(glm::vec3(m[2]));
        const glm::vec3 right = glm::normalize(glm::vec3(m[0]));

        Light l{};
        l.color = glm::vec4(color, intensity);
        const float shadow = castShadows ? 1.0f : 0.0f;

        switch (type)
        {
        case LightType::Directional:
            l.position = glm::vec4(fwd, 0.0f);
            break;
        case LightType::Point:
            l.position = glm::vec4(t.position, 1.0f);
            l.direction = glm::vec4(fwd, range);
            l.params = glm::vec4(0, 0, shadow, 0);
            break;
        case LightType::Spot:
        {
            const float outer = std::clamp(outerAngle, 1.0f, 89.0f);
            const float inner = std::clamp(innerAngle, 0.0f, outer - 0.5f);
            l.position = glm::vec4(t.position, 2.0f);
            l.direction = glm::vec4(fwd, range);
            l.params = glm::vec4(std::cos(glm::radians(inner)), std::cos(glm::radians(outer)), shadow, 0);
            break;
        }
        case LightType::Rect:
            l.position = glm::vec4(t.position, 3.0f);
            l.direction = glm::vec4(fwd, range);
            l.params = glm::vec4(size.x * 0.5f, size.y * 0.5f, shadow, 0);
            l.right = glm::vec4(right, twoSided ? 1.0f : 0.0f);
            break;
        }
        return l;
    }
};

inline void setLightRotation(Transform& t, glm::vec3 forward, glm::vec3 right = glm::vec3(0.0f))
{
    const glm::vec3 f = glm::normalize(forward);
    const float pitch = std::asin(std::clamp(f.y, -1.0f, 1.0f));
    const float yaw   = std::atan2(-f.x, -f.z);

    float roll = 0.0f;
    if (glm::dot(right, right) > 1e-8f)
    {
        const glm::vec3 r0(std::cos(yaw), 0.0f, -std::sin(yaw));
        const glm::vec3 u0(std::sin(pitch) * std::sin(yaw), std::cos(pitch), std::sin(pitch) * std::cos(yaw));
        roll = std::atan2(glm::dot(right, u0), glm::dot(right, r0));
    }
    t.rotation = glm::degrees(glm::vec3(pitch, yaw, roll));
}

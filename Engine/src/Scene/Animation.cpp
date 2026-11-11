#include "Scene/Animation.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

glm::vec3 Animation::interpolatePosition(const std::vector<PositionKey>& keys, float time)
{
    if (keys.empty()) return glm::vec3(0.0f);
    if (keys.size() == 1) return keys[0].value;

    for (size_t i = 0; i < keys.size() - 1; i++)
    {
        if (time < keys[i + 1].time)
        {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            t = glm::clamp(t, 0.0f, 1.0f);
            return glm::mix(keys[i].value, keys[i + 1].value, t);
        }
    }
    return keys.back().value;
}

glm::quat Animation::interpolateRotation(const std::vector<RotationKey>& keys, float time)
{
    if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (keys.size() == 1) return keys[0].value;

    for (size_t i = 0; i < keys.size() - 1; i++)
    {
        if (time < keys[i + 1].time)
        {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            t = glm::clamp(t, 0.0f, 1.0f);
            return glm::slerp(keys[i].value, keys[i + 1].value, t);
        }
    }
    return keys.back().value;
}

glm::vec3 Animation::interpolateScale(const std::vector<ScaleKey>& keys, float time)
{
    if (keys.empty()) return glm::vec3(1.0f);
    if (keys.size() == 1) return keys[0].value;

    for (size_t i = 0; i < keys.size() - 1; i++)
    {
        if (time < keys[i + 1].time)
        {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            t = glm::clamp(t, 0.0f, 1.0f);
            return glm::mix(keys[i].value, keys[i + 1].value, t);
        }
    }
    return keys.back().value;
}

glm::mat4 Animation::sampleJointTransform(const std::string& jointName, float time) const
{
    for (const auto& channel : channels)
    {
        if (channel.jointName == jointName)
        {
            glm::vec3 pos = interpolatePosition(channel.poisitonKeys, time);
            glm::quat rot = interpolateRotation(channel.rotationKeys, time);
            glm::vec3 scale = interpolateScale(channel.scaleKeys, time);

            glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
            glm::mat4 R = glm::toMat4(rot);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

            return T * R * S;
        }
    }
    return glm::mat4(1.0f);
}

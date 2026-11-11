#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <unordered_map>

struct PositionKey { float time; glm::vec3 value; };
struct RotationKey { float time; glm::quat value; };
struct ScaleKey { float time; glm::vec3 value;};

struct JointAnimationChannel
{
    std::string jointName;
    std::vector<PositionKey> poisitonKeys;
    std::vector<RotationKey> rotationKeys;
    std::vector<ScaleKey> scaleKeys;
};

class Animation
{
public:
    std::string name;
    float duration = 0.0f;
    std::vector<JointAnimationChannel> channels;

    glm::mat4 sampleJointTransform(const std::string& jointName, float time) const;

private:
    static glm::vec3 interpolatePosition(const std::vector<PositionKey>& keys, float time);
    static glm::quat interpolateRotation(const std::vector<RotationKey>& keys, float time);
    static glm::vec3 interpolateScale(const std::vector<ScaleKey>& keys, float time);
};

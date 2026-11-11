#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>

struct Joint
{
    std::string name;
    int parentIndex = -1;
    glm::mat4 inverseBindMatrix{1.0f};
    glm::mat4 localBindTransform{1.0f};
};

class Skeleton
{
public:
    int addJoint(const Joint& joint)
    {
        int index = (int)m_joints.size();
        m_joints.push_back(joint);
        m_nameToIndex[joint.name] = index;
        return index;
    }

    void setJointParent(int jointIndex, int parentIndex)
    {
        if (jointIndex >= 0 && jointIndex < (int)m_joints.size())
            m_joints[jointIndex].parentIndex = parentIndex;
    }

    int getJointIndex(const std::string& name) const
    {
        auto it = m_nameToIndex.find(name);
        return (it != m_nameToIndex.end()) ? it->second : -1;
    }

    const Joint& getJoint(int index) const { return m_joints[index]; }
    size_t getJointCount() const { return m_joints.size(); }
    const std::vector<Joint>& getJoints() const { return m_joints; }

private:
    std::vector<Joint> m_joints;
    std::unordered_map<std::string, int> m_nameToIndex;
};

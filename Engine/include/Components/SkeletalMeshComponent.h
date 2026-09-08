#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Scene/SkeletalMesh.h"
#include "Core/Log.h"
#include "Scene/Animation.h"
#include "Vulkan/BoneMatrixBuffer.h"
#include "ASH/ASH.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class SkeletalMeshComponent : public IComponent
{
public:
    SkeletalMeshComponent(ASH::Device* device, uint32_t frameCount)
    {
        m_boneBuffer.initialize(device, frameCount);
    }

    ~SkeletalMeshComponent()
    {
        m_boneBuffer.cleanup();
    }

    ASH::Buffer* getBoneBuffer(uint32_t frameIndex) const
    {
        return m_boneBuffer.getBuffer(frameIndex);
    }

    void setSkeletalMesh(std::shared_ptr<SkeletalMesh> mesh) { m_skeletalMesh = mesh; }
    std::shared_ptr<SkeletalMesh> getSkeletalMesh() const { return m_skeletalMesh; }

    void play(std::shared_ptr<Animation> animation, bool loop = true)
    {
        m_currentAnimation = animation;
        m_currentTime = 0.0f;
        m_loop = loop;
        m_playing = true;
    }

    bool playAnimationByName(GameObject& owner, const std::string& name, bool loop = true)
    {
        auto it = owner.animations.find(name);
        if (it == owner.animations.end())
        {
            LOG("playAnimationByName: animation '" << name << "' not found");
            return false;
        }
        play(it->second, loop);
        return true;
    }

    void stop() { m_playing = false; }
    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void update(float deltaTime) override
    {
        if (!m_skeletalMesh || !m_skeletalMesh->getSkeleton()) return;
        if (m_playing && m_currentAnimation)
        {
            m_currentTime += deltaTime;
            if (m_currentTime > m_currentAnimation->duration)
            {
                if (m_loop)
                    m_currentTime = fmod(m_currentTime, m_currentAnimation->duration);
                else
                {
                    m_currentTime = m_currentAnimation->duration;
                    m_playing = false;
                }
            }
        }
        computePose();
    }

    void uploadBoneMatrices(uint32_t frameIndex)
    {
        BoneMatrixData data{};
        size_t count = std::min(m_boneMatrices.size(), (size_t)MAX_BONES);
        for (size_t i = 0; i < count; i++)
            data.bones[i] = m_boneMatrices[i];
        m_boneBuffer.update(frameIndex, data);
    }

private:
    void computePose()
    {
        const Skeleton& skeleton = *m_skeletalMesh->getSkeleton();
        size_t jointCount = skeleton.getJointCount();
        if (m_globalTransforms.size() != jointCount)
        {
            m_globalTransforms.resize(jointCount);
            m_boneMatrices.resize(jointCount, glm::mat4(1.0f));
        }
        for (size_t i = 0; i < jointCount; i++)
        {
            const Joint& joint = skeleton.getJoint((int)i);
            glm::mat4 localTransform = m_currentAnimation
                ? m_currentAnimation->sampleJointTransform(joint.name, m_currentTime)
                : joint.localBindTransform;
            if (joint.parentIndex >= 0)
                m_globalTransforms[i] = m_globalTransforms[joint.parentIndex] * localTransform;
            else
                m_globalTransforms[i] = localTransform;
            m_boneMatrices[i] = m_globalTransforms[i] * joint.inverseBindMatrix;
        }
    }

    GameObject* m_owner = nullptr;
    std::shared_ptr<SkeletalMesh> m_skeletalMesh;
    std::shared_ptr<Animation> m_currentAnimation;
    float m_currentTime = 0.0f;
    bool m_loop = true;
    bool m_playing = false;
    std::vector<glm::mat4> m_globalTransforms;
    std::vector<glm::mat4> m_boneMatrices;
    BoneMatrixBuffer m_boneBuffer;
};

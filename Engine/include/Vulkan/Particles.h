#pragma once

#include "ASH/ASH.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/ComputePipeline.h"
#include "Vulkan/Texture.h"
#include "Vulkan/DecalCameraBuffer.h"
#include "Scene/ParticlesLoader.h"
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Particles
{
public:
    struct Emitter
    {
        std::string name;
        uint32_t effect = 0;
        uint32_t outBase = 0;
        uint32_t count = 0;
        bool additive = false;
        glm::vec3 offset{0.0f};
        float startTime = 0.0f;
        bool active = true;
    }; 

    void initialize(ASH::Device* device, ASH::RenderPass* renderPass, uint32_t frameCount, ASH::Texture* depthTexture, ASH::Sampler* depthSampler, const DecalCameraBuffer& cameraBuffer);
    void cleanup();

    bool load(const std::string& path);
    void unload();

    void rebuildDescriptors(ASH::Texture* depthTexture, ASH::Sampler* depthSampler);

    void recordCompute(ASH::CommandBuffer* cmd, uint32_t frameIndex, float totalTime);

    void recordDraw(ASH::CommandBuffer* cmd, uint32_t frameIndex);

    bool hasWork() const;
    float now() const { return m_time; }

    int findEmitter(const std::string& name) const;
    Emitter* getEmitter(int index) { return (index >= 0 && index < (int)m_emitters.size()) ? &m_emitters[index] : nullptr; }
    size_t emitterCount() const { return m_emitters.size(); }

private:
    struct ComputePush  { glm::vec4 offsetTime; uint32_t info[4]; };
    struct DrawPush { uint32_t info[4]; };
    static constexpr uint32_t kInstanceStride = sizeof(float) * 12;

    void createDescriptorSets();
    Texture* loadSprite(const std::string& rawName, const std::string& baseDir);

    ASH::Device* m_device = nullptr;
    ASH::RenderPass* m_renderPass = nullptr;
    const DecalCameraBuffer* m_camera = nullptr;
    ASH::Texture* m_depthTexture = nullptr;
    ASH::Sampler* m_depthSampler = nullptr;
    uint32_t m_frameCount = 1;
    float m_time = 0.0f;

    ComputePipeline m_compute;
    Pipeline m_drawAlpha;
    Pipeline m_drawAdditive;

    std::vector<ParticleEffectRecord> m_records;
    std::vector<Emitter> m_emitters;

    std::unique_ptr<ASH::Buffer> m_effectBuffer;
    std::vector<std::unique_ptr<ASH::Buffer>> m_outBuffers;
    uint32_t m_totalSlots = 0;

    std::vector<std::unique_ptr<ASH::DescriptorSet>> m_computeSets;
    std::vector<std::vector<std::unique_ptr<ASH::DescriptorSet>>> m_drawSets;

    Texture m_defaultSprite;
    std::map<std::string, std::unique_ptr<Texture>> m_sprites;
    std::vector<Texture*> m_emitterSprite;
};
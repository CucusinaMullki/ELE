#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Vulkan/Renderer.h"
#include "Vulkan/DynamicVoxelProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include <memory>

class SkeletalVoxelProxyComponent : public IComponent
{
public:
    SkeletalVoxelProxyComponent(Renderer& renderer, SkeletalMesh* mesh, Material* material, ASH::Buffer* boneBuffer, uint32_t rebakeInterval = 6)
        : m_renderer(renderer), m_mesh(mesh), m_material(material), m_boneBuffer(boneBuffer), m_rebakeInterval(rebakeInterval)
    {
        m_proxy = renderer.createSkeletalVoxelProxy(mesh, material, boneBuffer);
    }

    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void update(float deltaTime) override
    {
        if (!m_proxy) return;

        m_frameCounter++;
        if (m_frameCounter >= m_rebakeInterval)
        {
            m_renderer.rebakeSkeletalVoxelProxy(*m_proxy, m_mesh, m_material, m_boneBuffer);
            m_frameCounter = 0;
        }

        m_renderer.stampVoxelProxy(*m_proxy, m_owner->transform.position);
    }

private:
    Renderer& m_renderer;
    GameObject* m_owner = nullptr;
    SkeletalMesh* m_mesh;
    Material* m_material;
    ASH::Buffer* m_boneBuffer;
    std::unique_ptr<DynamicVoxelProxy> m_proxy;
    uint32_t m_rebakeInterval;
    uint32_t m_frameCounter = 0;
};
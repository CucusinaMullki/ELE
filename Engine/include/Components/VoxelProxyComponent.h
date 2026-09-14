#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include "Vulkan/Renderer.h"
#include "Vulkan/DynamicVoxelProxy.h"
#include <memory>

class VoxelProxyComponent : public IComponent
{
public:
    VoxelProxyComponent(Renderer& renderer, Mesh* mesh, Material* material)
        : m_renderer(renderer)
    {
        m_proxy = renderer.createVoxelProxy(mesh, material);
    }

    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void update(float deltaTime) override
    {
        if (!m_proxy) return;
        m_renderer.stampVoxelProxy(*m_proxy, m_owner->transform.position);
    }

    DynamicVoxelProxy* getProxy() const { return m_proxy.get(); }

private:
    Renderer& m_renderer;
    GameObject* m_owner = nullptr;
    std::unique_ptr<DynamicVoxelProxy> m_proxy;
};
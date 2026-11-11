#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"

class MeshComponent : public IComponent
{
public:
    void onAttach(GameObject& owner) override { m_owner = &owner; }

    std::shared_ptr<Mesh> getMesh() const { return m_owner->mesh; }
    void setMesh(std::shared_ptr<Mesh> mesh) { m_owner->mesh = mesh; }

    const std::vector<std::shared_ptr<Material>>& getMaterial() const { return m_owner->materials; }

private:
    GameObject* m_owner = nullptr;
};

#pragma once
#include "Scene/Transform.h"
#include "Scene/Mesh.h"
#include "Scene/Material.h"
#include <memory>
#include <vector>

class GameObject
{
public:
    Transform transform;
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    bool isDecal = false;

    void setMaterial(std::shared_ptr<Material> mat) { materials = { mat }; }

    std::shared_ptr<Material> getMaterial(int index) const
    {
        if (materials.empty()) return nullptr;
        if (index < 0 || index >= (int)materials.size()) return materials[0];
        return materials[index];
    }
};
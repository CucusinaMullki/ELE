#pragma once
#include "Scene/Transform.h"
#include "Scene/Mesh.h"
#include "Scene/Material.h"
#include <memory>

class GameObject
{
public:
    Transforn transform;

    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
};
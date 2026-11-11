#pragma once
#include "Scene/IComponent.h"
#include <string>

class SpawnPointComponent : public IComponent
{
public:
    std::string spawnType = "default";

    SpawnPointComponent() = default;
    explicit SpawnPointComponent(std::string type) : spawnType(std::move(type)) {}

    const char* typeName() const override { return "SpawnPoint"; }
    void visit(PropertyVisitor& v) override { v.field("spawnType", spawnType); }
};

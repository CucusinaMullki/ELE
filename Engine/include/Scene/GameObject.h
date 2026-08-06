#pragma once
#include "Scene/Transform.h"
#include "Scene/Mesh.h"
#include "Scene/Material.h"
#include "Scene/IComponent.h"
#include <memory>
#include <vector>
#include <typeindex>

class GameObject
{
public:
    Transform transform;
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    bool isDecal = false;
    bool isDynamic = false;

    void setMaterial(std::shared_ptr<Material> mat) { materials = { mat }; }
    std::shared_ptr<Material> getMaterial(int index) const
    {
        if (materials.empty()) return nullptr;
        if (index < 0 || index >= (int)materials.size()) return materials[0];
        return materials[index];
    }

    template<typename T, typename... Args>
    T& addComponent(Args&&... args)
    {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *comp;
        comp->onAttach(*this);
        m_components[std::type_index(typeid(T))] = std::move(comp);
        return ref;
    }

    template<typename T>
    T* getComponent()
    {
        auto it = m_components.find(std::type_index(typeid(T)));
        if (it == m_components.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    template<typename T>
    bool hasComponent() const
    {
        return m_components.find(std::type_index(typeid(T))) != m_components.end();
    }

    template<typename T>
    void removeComponent()
    {
        m_components.erase(std::type_index(typeid(T)));
    }

    void updateComponents(float deltaTime)
    {
        for (auto& [type, comp] : m_components)
            comp->update(deltaTime);
    }

    void fixedUpdateComponents(float fixedDeltaTime)
    {
        for (auto& [type, comp] : m_components)
            comp->fixedUpdate(fixedDeltaTime);
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponent>> m_components;

};
#pragma once
#include "Scene/GameObject.h"
#include <vector>
#include <memory>

class Scene
{
public:
    GameObject& addObject()
    {
        m_objects.push_back(std::make_shared<GameObject>());
        return *m_objects.back();
    }

    void removeObject(const std::shared_ptr<GameObject>& obj)
    {
        m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());
    }

    const std::vector<std::shared_ptr<GameObject>>& getObjects() const { return m_objects; }

    void updateComponents(float deltaTime)
    {
        for (auto& obj : m_objects)
            obj->updateComponents(deltaTime);
    }

    void fixedUpdateComponents(float fixedDeltaTime)
    {
        for (auto& obj : m_objects)
            obj->fixedUpdateComponents(fixedDeltaTime);
    }
    
private:
    std::vector<std::shared_ptr<GameObject>> m_objects;
};
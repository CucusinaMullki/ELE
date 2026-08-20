#pragma once
#include "Scene/GameObject.h"
#include <vector>
#include <memory>

class Scene
{
public:
    GameObject& addObject()
    {
        m_pendingObjects.push_back(std::make_shared<GameObject>());
        return *m_pendingObjects.back();
    }

    void removeObject(const std::shared_ptr<GameObject>& obj)
    {
        m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());
    }

    void flush() { flushPenging(); }

    const std::vector<std::shared_ptr<GameObject>>& getObjects() const { return m_objects; }

    GameObject* findByName(const std::string& name) const
    {
        for (const auto& obj : m_objects)
            if (obj->name == name)
                return obj.get();
        return nullptr;
    }

    void updateComponents(float deltaTime)
    {
        flushPenging();
        for (auto& obj : m_objects)
            obj->updateComponents(deltaTime);
    }

    void fixedUpdateComponents(float fixedDeltaTime)
    {
        flushPenging();
        for (auto& obj : m_objects)
            obj->fixedUpdateComponents(fixedDeltaTime);
        flushPenging();
    }
    
    std::vector<GameObject*> findAllByType(const std::string& type) const
    {
        std::vector<GameObject*> result;
        for (const auto& obj : m_objects)
            if (obj->entityType == type)
                result.push_back(obj.get());
        return result;
    }
    
private:
    void flushPenging()
    {
        if (m_pendingObjects.empty()) return;
        for (auto& obj : m_pendingObjects)
            m_objects.push_back(std::move(obj));
        m_pendingObjects.clear();
    }

    std::vector<std::shared_ptr<GameObject>> m_objects;
    std::vector<std::shared_ptr<GameObject>> m_pendingObjects;
};
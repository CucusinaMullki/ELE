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

    const std::vector<std::shared_ptr<GameObject>>& getObjects() const { return m_objects; }
    
private:
    std::vector<std::shared_ptr<GameObject>> m_objects;
};
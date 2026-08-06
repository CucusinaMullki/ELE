#pragma once
#include "Scene/IComponent.h"
#include "Scene/GameObject.h"
#include <glm/glm.hpp>

class TransformComponent : public IComponent
{
public:
    void onAttach(GameObject& owner) override { m_owner = &owner; }

    void setPosition(glm::vec3 pos) { m_owner->transform.position = pos; }
    void setRotationEuler(glm::vec3 eulerDegrees) { m_owner->transform.rotation = eulerDegrees; }
    void setScale(glm::vec3 scale) { m_owner->transform.scale = scale; }

    glm::vec3 getPosition() const { return m_owner->transform.position; }
    glm::vec3 getRotationEuler() const { return m_owner->transform.rotation; }
    glm::mat4 getWorldMatrix() const { return m_owner->transform.getMatrix(); }

private:
    GameObject* m_owner = nullptr;
};
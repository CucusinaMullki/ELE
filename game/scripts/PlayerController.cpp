#include "Interactable.h"
#include "Scripting/ScriptModule.h"
#include "Core/Engine.h"
#include "Components/CharacterComponent.h"
#include "Components/CameraComponent.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

class PlayerController : public Script
{
public:
    ELE_SCRIPT(PlayerController)

    float moveSpeed = 4.0f;
    float sprintMultiplier = 1.7f;
    float jumpSpeed = 5.0f;
    float mouseSensitivity = 0.1f;

    void visit(PropertyVisitor& v) override
    {
        v.field("moveSpeed", moveSpeed);
        v.field("sprintMultiplier", sprintMultiplier);
        v.field("jumpSpeed", jumpSpeed);
        v.field("mouseSensitivity", mouseSensitivity);
    }

    void onStart() override
    {
        if (auto* ch = owner().getComponent<CharacterComponent>())
            ch->teleport(transform().position);
    }

    void onUpdate(float dt) override
    {
        auto* ch  = owner().getComponent<CharacterComponent>();
        auto* cam = owner().getComponent<CameraComponent>();
        if (!ch || !cam) return;

        Input& in = engine().getInput();

        transform().rotation.y += in.getMouseDeltaX() * mouseSensitivity;
        cam->setPitch(std::clamp(cam->getPitch() - in.getMouseDeltaY() * mouseSensitivity * -1.0f, -89.0f, 89.0f));

        const float yaw = glm::radians(transform().rotation.y);
        const glm::vec3 forward(std::sin(yaw), 0.0f, -std::cos(yaw));
        const glm::vec3 right(std::cos(yaw), 0.0f, std::sin(yaw));

        glm::vec2 axis(0.0f);
        if (in.isKeyPressed(Key::W)) axis.y += 1.0f;
        if (in.isKeyPressed(Key::S)) axis.y -= 1.0f;
        if (in.isKeyPressed(Key::D)) axis.x += 1.0f;
        if (in.isKeyPressed(Key::A)) axis.x -= 1.0f;
        axis.x += in.gamepad().getAxis(GamepadAxis::LeftX);
        axis.y -= in.gamepad().getAxis(GamepadAxis::LeftY);
        if (glm::length(axis) > 1.0f) axis = glm::normalize(axis);

        float speed = moveSpeed;
        if (in.isKeyPressed(Key::LeftShift)) speed *= sprintMultiplier;

        ch->setMoveInput((forward * axis.y + right * axis.x) * speed);

        if (in.isKeyJustPressed(Key::Space) || in.isActionJustPressed("Up"))
            ch->jump(jumpSpeed);

        if (in.isKeyJustPressed(Key::E) || in.isActionJustPressed("Interact"))
            if (Interactable* target = findTarget(forward))
                target->interact(owner());

        ch->syncTransform(dt);
        cam->applyToCamera();
    }

private:
    Interactable* findTarget(const glm::vec3& forward)
    {
        const glm::vec3 me = transform().position;
        Interactable* best = nullptr;
        float bestDist = 1e9f;

        for (const auto& obj : engine().getScene().getObjects())
        {
            if (obj.get() == &owner()) continue;
            obj->forEachComponent([&](IComponent& c) {
                auto* i = dynamic_cast<Interactable*>(&c);
                if (!i) return;

                glm::vec3 to = obj->transform.position - me;
                to.y = 0.0f;
                const float d = glm::length(to);
                if (d > i->interactDistance || d >= bestDist) return;
                if (d > 0.3f && glm::dot(to / d, forward) < 0.5f) return;

                best = i;
                bestDist = d;
            });
        }
        return best;
    }
};
ELE_REGISTER_SCRIPT(PlayerController)

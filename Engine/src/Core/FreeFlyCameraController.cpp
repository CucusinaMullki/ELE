#include "Core/FreeFlyCameraController.h"
#include "Core/Camera.h"
#include "Core/Input.h"
#include "Core/IGamepad.h"
#include <glm/glm.hpp>

void FreeFlyCameraController::update(Camera& camera, const Input& input, float deltaTime)
{
    glm::vec3 moveDir(0.0f);
    if (input.isKeyPressed(Key::W)) moveDir += camera.getFront();
    if (input.isKeyPressed(Key::S)) moveDir -= camera.getFront();
    if (input.isKeyPressed(Key::A)) moveDir -= camera.getRight();
    if (input.isKeyPressed(Key::D)) moveDir += camera.getRight();

    if (input.isActionPressed("Up"))   moveDir += camera.getUp();
    if (input.isActionPressed("Down")) moveDir -= camera.getUp();

    moveDir += camera.getFront() * (-input.gamepad().getAxis(GamepadAxis::LeftY))
             + camera.getRight() * input.gamepad().getAxis(GamepadAxis::LeftX);

    if (glm::length(moveDir) > 0.0f)
        camera.processKeyboard(glm::normalize(moveDir), deltaTime);

    float gamepadYaw = input.gamepad().getAxis(GamepadAxis::RightX) * 150.0f * deltaTime * 10.0f;
    float gamepadPitch = input.gamepad().getAxis(GamepadAxis::RightY) * 150.0f * deltaTime * 10.0f;

    camera.processMouse(input.getMouseDeltaX() + gamepadYaw, input.getMouseDeltaY() - gamepadPitch);
}
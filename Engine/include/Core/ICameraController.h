#pragma once

class Camera;
class Input;

class ICameraController
{
public:
    virtual ~ICameraController() = default;
    virtual void update(Camera& camera, const Input& input, float deltaTime) = 0;
    virtual void onActivate(Camera& camera) {}
};
#pragma once
#include "Core/ICameraController.h"

class FreeFlyCameraController : public ICameraController
{
public:
    void update(Camera& camera, const Input& input, float deltaTime) override;
};
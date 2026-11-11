#pragma once

enum class GamepadButton
{
    A, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DPadUp, DPadRight, DPadDown, DPadLeft,
    Unknown
};

enum class GamepadAxis
{
    LeftX, LeftY,
    RightX, RightY,
    LeftTrigger, RightTrigger
};

class IGamepad
{
public:
    virtual ~IGamepad() = default;

    virtual void update() = 0;

    virtual bool isConnected() const = 0;
    virtual bool isButtonPressed(GamepadButton button) const = 0;
    virtual bool isButtonJustPressed(GamepadButton button) const = 0;

    virtual float getAxis(GamepadAxis axis) const = 0;
};

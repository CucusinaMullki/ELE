#include "Core/Engine.h"

void InputInScene(Engine& engine)
{
    engine.getInput().bindAction("Up", InputBinding::fromKey(Key::E));
    engine.getInput().bindAction("Up", InputBinding::fromGamepad(GamepadButton::RightBumper));

    engine.getInput().bindAction("Down", InputBinding::fromKey(Key::Q));
    engine.getInput().bindAction("Down", InputBinding::fromGamepad(GamepadButton::LeftBumper));

    engine.getInput().bindAction("Fire", InputBinding::fromMouse(MouseButton::Left));
    engine.getInput().bindAction("Fire", InputBinding::fromGamepad(GamepadButton::B));
}

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);

    InputInScene(engine);

    auto& character = engine.loadGltfObject("assets/models/Level.glb");
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
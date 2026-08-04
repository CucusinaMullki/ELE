#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);

    engine.getInput().bindAction("Fire", InputBinding::fromMouse(MouseButton::Left));
    engine.getInput().bindAction("Fire", InputBinding::fromGamepad(GamepadButton::A));

    auto& character = engine.loadGltfObject("assets/models/Level.glb");
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
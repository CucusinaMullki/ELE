#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);
    engine.setExposure(0.25f);

    auto& character = engine.loadGltfObject("assets/models/BlenderModel.glb");
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
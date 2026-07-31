#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);
    engine.setExposure(0.25f);
    FogSettings fog;
    fog.enabled = true;
    engine.setFogSettings(fog);

    GodRaySettings godrays;
    godrays.enabled = true;
    godrays.intensity = 1.0f;
    engine.setFogSettings(fog);
    engine.setGodRaySettings(godrays);
    auto& character = engine.loadGltfObject("assets/models/GodRaysTestModel.glb");
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
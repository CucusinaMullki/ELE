#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);
    engine.setExposure(0.25f);
    FogSettings fog;
    fog.enabled = true;
    fog.density = 0.25f;
    fog.scatteringColor = glm::vec3(0.36f, 0.36f ,0.36f);
    engine.setFogSettings(fog);

    GodRaySettings godrays;
    godrays.enabled = true;
    godrays.intensity = 1.0f;
    engine.setFogSettings(fog);
    engine.setGodRaySettings(godrays);
    auto& character = engine.loadGltfObject("assets/models/Level.glb");
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
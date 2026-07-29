#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::None);
    engine.setExposure(0.25f);

    auto& character = engine.loadGltfObject("assets/models/BlenderModel.glb");
    engine.addScreenSpaceDecal("assets/decals/blood.png",
        glm::vec3(-1.5f, 0.05f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.5f, 1.0f)); 
    //////////////////////////////
    engine.addPointLight(glm::vec3(-1.5f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), .0f);
    engine.run();

    engine.shutdown();
    return 0;
}
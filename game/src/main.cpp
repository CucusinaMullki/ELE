#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");

    auto& character = engine.loadGltfObject("assets/models/BlenderModel.glb");
    character.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    //////////////////////////////
        
    engine.run();

    engine.shutdown();
    return 0;
}
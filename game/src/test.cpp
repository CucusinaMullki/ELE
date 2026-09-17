#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);
    engine.setExposure(1.0f);

    engine.getRenderer().enableVoxelGI(8.0f, 32, 2);

    auto& scene = engine.loadGltfObject("assets/models/BlenderModel.glb");
    engine.getScene().flush();
    engine.setCameraControlMode(true);

    engine.run();

    engine.shutdown();
    return 0;
}
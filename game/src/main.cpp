#include "Core/Engine.h"
#include "Core/Log.h"
#include "Scene/ComponentRegistry.h"
#include "UI/DebugConsoleWidget.h"
#include "Components/ParticlesComponent.h"
#include "DebugConsoleController.h"

extern "C" uint32_t ele_register_scripts(ComponentRegistry* r, const char** names, uint32_t cap);

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);
    engine.getRenderer().setVoxelGIEnabled(false);
    engine.setCameraControlMode(false);

    const uint32_t scriptCount = ele_register_scripts(&ComponentRegistry::instance(), nullptr, 0);
    LOG("scripts registered: " << scriptCount);

    engine.loadScene("Levels/Level.escene");

    auto& console = engine.getUI().addWidget<DebugConsoleWidget>();
    console.setPosition(glm::vec2(10, 10));
    console.setSize(glm::vec2(600, 400));
    console.setVisible(false);

    if (GameObject* player = engine.getScene().findByName("Player"))
        player->addComponent<DebugConsoleController>(engine.getInput(), console);
    else
        LOG("main: no 'Player' in Levels/Level.escene — add it in the editor");

    engine.run();

    engine.shutdown();
    return 0;
}

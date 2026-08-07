#include "Core/Engine.h"
#include "Components/MeshComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RigidBodyComponent.h"

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

    auto mesh = engine.loadMesh("assets/models/cube.glb");
    auto material = engine.createDefaultMaterial();

    auto Dickmesh = engine.loadMesh("assets/models/Dick.glb", true);

    auto& box = engine.getScene().addObject();
    box.transform.position = glm::vec3(0, 10, 0);
    box.setMaterial(material);
    box.addComponent<MeshComponent>().setMesh(mesh);
    box.addComponent<TransformComponent>();
    box.addComponent<RigidBodyComponent>(engine.getPhysics(),
        RigidBodyComponent::Shape::Box, RigidBodyComponent::Motion::Dynamic, nullptr,
        glm::vec3(1.0f), 0.5f);

    auto& Dick = engine.getScene().addObject();
    Dick.transform.position = glm::vec3(0, 20, 0);
    Dick.setMaterial(material);
    Dick.addComponent<MeshComponent>().setMesh(Dickmesh);
    Dick.addComponent<TransformComponent>();
    Dick.addComponent<RigidBodyComponent>(engine.getPhysics(),
        RigidBodyComponent::Shape::ConvexHull, RigidBodyComponent::Motion::Dynamic, Dickmesh,
        glm::vec3(1.0f), 1.5f);
    
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
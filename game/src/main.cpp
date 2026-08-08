#include "Core/Engine.h"
#include "Components/MeshComponent.h"
#include "PlayerComponent.h"
#include "Components/InputComponent.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RigidBodyComponent.h"


GameObject* createCube(Engine& engine, glm::vec3 pos)
{
    auto& cube = engine.getScene().addObject();
    auto material = engine.createDefaultMaterial();
    cube.addComponent<TransformComponent>().setPosition(pos);
    cube.setMaterial(material);
    cube.addComponent<MeshComponent>().setMesh(engine.loadMesh("assets/models/Dick.glb"));
    cube.addComponent<RigidBodyComponent>(engine.getPhysics(),
        RigidBodyComponent::Shape::ConvexHull, RigidBodyComponent::Motion::Dynamic, engine.loadMesh("assets/models/Dick.glb", true),
        glm::vec3(1.0f), 1.5f);
    return &cube;
}

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

    auto& player = engine.getScene().addObject();
    player.transform.position = glm::vec3(0, 0, 0);

    player.addComponent<TransformComponent>();
    player.addComponent<InputComponent>(engine.getInput());
    player.addComponent<CharacterComponent>(engine.getPhysics(), engine.getTime(), 0.3f, 1.4f, 60.0f);
    player.addComponent<CameraComponent>(engine.getCamera(), CameraComponent::Mode::FirstPerson);
    player.addComponent<PlayerComponent>();

    engine.setCameraControlMode(false);

    auto& character = engine.loadGltfObject("assets/models/Level.glb");
    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
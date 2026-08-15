#include "Core/Engine.h"
#include "Core/Log.h"
#include "Components/MeshComponent.h"
#include "PlayerComponent.h"
#include "Components/InputComponent.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RigidBodyComponent.h"
#include "Components/CharacterComponent.h"
#include "Components/TriggerComponent.h"


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
    engine.getInput().bindAction("Up", InputBinding::fromKey(Key::Space));
    engine.getInput().bindAction("Up", InputBinding::fromGamepad(GamepadButton::RightBumper));

    engine.getInput().bindAction("Down", InputBinding::fromKey(Key::Q));
    engine.getInput().bindAction("Down", InputBinding::fromGamepad(GamepadButton::LeftBumper));

    engine.getInput().bindAction("Fire", InputBinding::fromMouse(MouseButton::Left));
    engine.getInput().bindAction("Fire", InputBinding::fromGamepad(GamepadButton::B));
    
    engine.getInput().bindAction("Interact", InputBinding::fromKey(Key::E));
}

GameObject* createInteractableCube(Engine& engine, glm::vec3 pos, std::function<void()> onInteract)
{
    auto& cube = engine.getScene().addObject();
    cube.name = "InteractButton";
    auto material = engine.createDefaultMaterial();
    auto mesh = engine.loadMesh("assets/models/cube.glb");

    cube.addComponent<TransformComponent>().setPosition(pos);
    cube.setMaterial(material);
    cube.addComponent<MeshComponent>().setMesh(mesh);

    cube.addComponent<RigidBodyComponent>(engine.getPhysics(),
        RigidBodyComponent::Shape::Box, RigidBodyComponent::Motion::Static, nullptr,
        glm::vec3(1.0f), 0.0f);

    auto& interact = cube.addComponent<InteractableComponent>();
    interact.onInteract = onInteract;

    return &cube;
}

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);

    InputInScene(engine);

    auto& character = engine.loadGltfObject("assets/models/Level.glb");
    engine.getScene().flush();

    auto& player = engine.getScene().addObject();

    GameObject* spawnPoint = engine.getScene().findByName("PlayerSpawn");
    LOG("spawnPoint found: " << (spawnPoint != nullptr));
    if (spawnPoint)
    {
        player.transform.position = spawnPoint->transform.position;
        LOG("player spawned at: " << player.transform.position.x << "," 
            << player.transform.position.y << "," << player.transform.position.z);
    }

    player.addComponent<TransformComponent>();
    player.addComponent<InputComponent>(engine.getInput());
    player.addComponent<CharacterComponent>(engine.getPhysics(), engine.getTime(), 0.3f, 1.4f, 60.0f);
    player.addComponent<CameraComponent>(engine.getCamera(), CameraComponent::Mode::FirstPerson);
    player.addComponent<HeadBobComponent>(glm::vec3(0.0f, 1.6f, 0.0f));
    player.addComponent<PlayerComponent>(engine.getPhysics());

    createInteractableCube(engine, glm::vec3(2.0f, 1.0f, 2.0f), [&engine]()
    {
        createCube(engine, glm::vec3(0.0f, 10.0f, 0.0f));
    });
    engine.setCameraControlMode(false);

    //////////////////////////////
    
    engine.run();

    engine.shutdown();
    return 0;
}
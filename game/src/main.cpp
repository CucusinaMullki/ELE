#include "Core/Engine.h"
#include "Core/Log.h"
#include "Components/MeshComponent.h"
#include "UI/TextWidget.h"
#include "Components/DoorComponent.h"
#include "Components/PlayerComponent.h"
#include "Components/InputComponent.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"
#include "Components/RigidBodyComponent.h"
#include "Components/CharacterComponent.h"
#include "AI/AIController.h"
#include "AI/EnemyBehaviorCommand.h"
#include "Components/AudioComponent.h"
#include "Components/TriggerComponent.h"
#include "UI/DebugConsoleWidget.h"
#include "DebugConsoleController.h"


//GameObject* createCube(Engine& engine, glm::vec3 pos)
//{
//    auto& cube = engine.getScene().addObject();
//    auto material = engine.createDefaultMaterial();
//
//    engine.getAudio().loadSound("test", "audio/test.wav");
//
//    cube.addComponent<TransformComponent>().setPosition(pos);
//    cube.setMaterial(material);
//    auto mesh = engine.loadMesh("assets/models/cube.glb");
//    cube.addComponent<MeshComponent>().setMesh(mesh);
//    cube.addComponent<RigidBodyComponent>(engine.getPhysics(),
//        RigidBodyComponent::Shape::ConvexHull, RigidBodyComponent::Motion::Dynamic, engine.loadMesh("assets/models/cube.glb", true),
//        glm::vec3(1.0f), 1.5f);
//    auto& audioComp = cube.addComponent<AudioComponent>(engine.getAudio(), engine.getPhysics(), "test");
//    audioComp.play();
//
//
//    return &cube;
//}

void InputInScene(Engine& engine)
{
    engine.getInput().bindAction("ToggleConsole", InputBinding::fromKey(Key::GraveAccent));

    engine.getInput().bindAction("Up", InputBinding::fromKey(Key::Space));
    engine.getInput().bindAction("Up", InputBinding::fromGamepad(GamepadButton::RightBumper));

    engine.getInput().bindAction("Down", InputBinding::fromKey(Key::Q));
    engine.getInput().bindAction("Down", InputBinding::fromGamepad(GamepadButton::LeftBumper));

    engine.getInput().bindAction("Fire", InputBinding::fromMouse(MouseButton::Left));
    engine.getInput().bindAction("Fire", InputBinding::fromGamepad(GamepadButton::B));
    
    engine.getInput().bindAction("Interact", InputBinding::fromKey(Key::E));
}

//GameObject* createInteractableCube(Engine& engine, glm::vec3 pos, std::function<void()> onInteract)
//{
//    auto& cube = engine.getScene().addObject();
//    cube.name = "InteractButton";
//    auto material = engine.createDefaultMaterial();
//    auto mesh = engine.loadMesh("assets/models/cube.glb");
//
//    cube.addComponent<TransformComponent>().setPosition(pos);
//    cube.setMaterial(material);
//    cube.addComponent<MeshComponent>().setMesh(mesh);
//
//    cube.addComponent<RigidBodyComponent>(engine.getPhysics(),
//        RigidBodyComponent::Shape::Box, RigidBodyComponent::Motion::Static, nullptr,
//        glm::vec3(1.0f), 0.0f);
//
//    auto& interact = cube.addComponent<InteractableComponent>();
//    interact.onInteract = onInteract;
//
//    return &cube;
//}

GameObject* createDoor(Engine& engine, glm::vec3 pos, glm::vec3 rot, std::function<void()> onInteractExtra = nullptr)
{
    static bool soundLoaded = false;
    if (!soundLoaded)
    {
        engine.getAudio().loadSound("door_creak", "audio/door_creak.wav");
        soundLoaded = true;
    }

    auto& door = engine.getScene().addObject();
    door.name = "Door";
    door.transform.position = pos;
    door.transform.rotation = rot;
    door.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);


    auto material = engine.loadMaterial(
        "assets/textures/door/Door_1_BaseColor.png",
        "assets/textures/door/Door_1_Normal.png",
        "assets/textures/door/Door_1_Metallic.png",
        "assets/textures/door/Door_1_Roughness.png"
    );

    auto doorMesh = engine.loadMesh("assets/models/Door.glb");

    door.addComponent<TransformComponent>();
    door.setMaterial(material);
    door.addComponent<MeshComponent>().setMesh(doorMesh);

    door.addComponent<RigidBodyComponent>(engine.getPhysics(),
        RigidBodyComponent::Shape::ConvexHull, RigidBodyComponent::Motion::Kinematic, engine.loadMesh("assets/models/Door.glb", true),
        glm::vec3(1.0f), 0.0f);

    auto& doorComp = door.addComponent<DoorComponent>();
    doorComp.setOpenAngle(100.0f);
    doorComp.setOpenSpeed(3.0f);

    auto& audioComp = door.addComponent<AudioComponent>(engine.getAudio(), engine.getPhysics(), "door_creak");

    auto& interact = door.addComponent<InteractableComponent>();
    interact.onInteract = [&doorComp, &audioComp, onInteractExtra]()
    {
        doorComp.toggle();
        audioComp.play(0.6f);
        if (onInteractExtra) onInteractExtra();
    };

    return &door;
}

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");
    engine.setAntialiasing(AAMethod::TAA);
    InputInScene(engine);

    engine.getRenderer().setVoxelGIEnabled(false);

    IBLSettings iblSettings{};
    iblSettings.diffuseIntensity = 1.0;
    engine.setIBLSettings(iblSettings);

    auto& level = engine.loadLevel("Levels/Level.elvl");
    engine.getScene().flush();

    engine.getAudio().loadSound("music", "audio/yebimenya.wav");
    engine.getAudio().playSound("music", 0.5f);

    auto cube = engine.loadModel("assets/models/TestModel2.emodel");
    LOG("loadModel: mesh=" << (cube.mesh ? "valid" : "NULL") << " materials count=" << cube.materials.size());
    
    auto& cubeobj = engine.getScene().addObject();
    cubeobj.mesh = cube.mesh;
    for (const auto& sm : cubeobj.mesh->getSubmeshes())
    {
        LOG("submesh materialIndex=" << sm.materialIndex);
        if (sm.materialIndex >= 0 && sm.materialIndex < (int)cube.materials.size())
            cubeobj.setMaterial(cube.materials[sm.materialIndex]);
    }
    cubeobj.addComponent<TransformComponent>().setPosition(glm::vec3(2.0f, 0.0f, 0.0f));

    auto& console = engine.getUI().addWidget<DebugConsoleWidget>();
    console.setPosition(glm::vec2(10, 10));
    console.setSize(glm::vec2(600, 400));
    console.setVisible(false);

    auto& player = engine.getScene().addObject();

    player.addComponent<DebugConsoleController>(engine.getInput(), console);

    for (GameObject* spawnPoint : engine.getScene().findAllByType("PlayerSpawn"))
    {
        player.transform.position = spawnPoint->transform.position;
        LOG("player spawned at: " << player.transform.position.x << "," 
            << player.transform.position.y << "," << player.transform.position.z);
    }

    auto skinnedModel = engine.loadModel("assets/models/TestMesh.emodel");
    LOG("loadModel SKINNED: hasSkeleton=" << skinnedModel.hasSkeleton
        << " skeletalMesh=" << (skinnedModel.skeletalMesh ? "valid" : "NULL")
        << " materials count=" << skinnedModel.materials.size());

    auto& skinnedObj = engine.getScene().addObject();
    skinnedObj.addComponent<TransformComponent>().setPosition(glm::vec3(0.0f, -0.5f, 0.0f));

    auto& skelComp = skinnedObj.addComponent<SkeletalMeshComponent>(engine.getRenderer().getDevice(), engine.getRenderer().getFramesInFlight());
    skelComp.setSkeletalMesh(skinnedModel.skeletalMesh);

    if (skinnedModel.skeletalMesh)
    {
        for (const auto& sm : skinnedModel.skeletalMesh->getSubmeshes())
        {
            LOG("skinned submesh materialIndex=" << sm.materialIndex);
            if (sm.materialIndex >= 0 && sm.materialIndex < (int)skinnedModel.materials.size())
            {
                auto mat = skinnedModel.materials[sm.materialIndex];
                skinnedObj.setMaterial(mat);
                engine.createSkinnedMaterialDescriptorSet(mat, skelComp);
            }
        }

        skelComp.createShadowDescriptorSets(engine.getRenderer().getDevice(),
            engine.getRenderer().getSkinnedShadowDescriptorSetLayout(),
            engine.getRenderer().getFramesInFlight());
    }

    auto animData = std::make_shared<Animation>(engine.loadAnimation("assets/Animations/Animation.eanim"));
    LOG("Animation loaded: name=" << animData->name << " duration=" << animData->duration
        << " channels=" << animData->channels.size());

    for (const auto& ch : animData->channels)
       LOG("  animation channel joint: '" << ch.jointName << "'");

    if (skinnedModel.skeletalMesh && skinnedModel.skeletalMesh->getSkeleton())
    {
        auto skeleton = skinnedModel.skeletalMesh->getSkeleton();
        for (const auto& joint : skeleton->getJoints())
            LOG("  skeleton joint: '" << joint.name << "'");
    }
    skelComp.play(animData, true);

    player.addComponent<TransformComponent>();
    player.addComponent<InputComponent>(engine.getInput());
    player.addComponent<CharacterComponent>(engine.getPhysics(), engine.getTime(), 0.3f, 1.4f, 60.0f);
    player.addComponent<CameraComponent>(engine.getCamera(), CameraComponent::Mode::FirstPerson);
    player.addComponent<HeadBobComponent>(glm::vec3(0.0f, 1.6f, 0.0f));
    player.addComponent<PlayerComponent>(engine.getPhysics());

    //auto& testText = engine.getUI().addWidget<TextWidget>("Penis");
    //testText.setFontScale(10.0f);
    //createInteractableCube(engine, glm::vec3(2.0f, 1.0f, 2.0f), [&engine]()
    //{
    //    //createCube(engine, glm::vec3(0.0f, 10.0f, 0.0f));
    //});
    engine.setCameraControlMode(false);

    
    engine.run();

    cube.mesh.reset();
    cube.materials.clear();
    skinnedModel.skeletalMesh.reset();
    skinnedModel.materials.clear();

    engine.shutdown();
    return 0;
}
#include "Scene/ComponentRegistry.h"
#include "Scene/GameObject.h"
#include "Components/TransformComponent.h"
#include "Components/RigidBodyComponent.h"
#include "Components/TriggerComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CharacterComponent.h"
#include "Components/CameraComponent.h"
#include "Components/SpawnPointComponent.h"
#include "Components/LightComponent.h"

void registerEngineComponents(ComponentRegistry& r)
{
    r.add("Light", [](GameObject& o, EngineContext&) -> IComponent* {
        return &o.addComponent<LightComponent>();
    });

    r.add("Transform", [](GameObject& o, EngineContext&) -> IComponent* {
        return &o.addComponent<TransformComponent>();
    });

    r.add("RigidBody", [](GameObject& o, EngineContext& c) -> IComponent* {
        return &o.addComponent<RigidBodyComponent>(*c.physics,
            RigidBodyComponent::Shape::Box, RigidBodyComponent::Motion::Static,
            nullptr, glm::vec3(0.5f), 1.0f);
    });

    r.add("Trigger", [](GameObject& o, EngineContext& c) -> IComponent* {
        return &o.addComponent<TriggerComponent>(*c.physics, glm::vec3(0.5f));
    });

    r.add("SkeletalMesh", [](GameObject& o, EngineContext& c) -> IComponent* {
        return &o.addComponent<SkeletalMeshComponent>(c.device, c.framesInFlight);
    });

    r.add("SpawnPoint", [](GameObject& o, EngineContext&) -> IComponent* {
        return &o.addComponent<SpawnPointComponent>();
    });

    r.add("Audio", [](GameObject& o, EngineContext& c) -> IComponent* {
        return &o.addComponent<AudioComponent>(*c.audio, *c.physics);
    });

    r.add("Character", [](GameObject& o, EngineContext& c) -> IComponent* {
        return &o.addComponent<CharacterComponent>(*c.physics, *c.time, 0.3f, 1.4f, 60.0f);
    });

    r.add("Camera", [](GameObject& o, EngineContext& c) -> IComponent* {
        return &o.addComponent<CameraComponent>(*c.camera, CameraComponent::Mode::FirstPerson);
    });
}

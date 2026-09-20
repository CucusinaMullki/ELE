#pragma once
#include "Vulkan/Renderer.h"
#include "Core/ResourceManager.h"
#include "Core/Input.h"
#include "Core/Camera.h"
#include "Scene/Scene.h"
#include "Physics/PhysicsSystem.h"
#include "Scene/ScreenSpaceDecal.h"
#include "Core/FogSettings.h"
#include "UI/UISystem.h"
#include "Core/ICameraController.h"
#include "Nav/NavMesh.h"
#include "Core/ModelLoader.h"
#include "Core/FreeFlyCameraController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TimeSystem.h"
#include "Audio/AudioSystem.h"
#include "Core/GltfModel.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <string>

struct LoadedModel
{
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
};

class Engine
{
public:
    void initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();

    void addDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void addPointLight(glm::vec3 position, glm::vec3 color, float intensity);

    std::shared_ptr<Mesh> loadMesh(const std::string& path, bool keepCpuData = false);
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
        const std::string& metallic, const std::string& roughness);
    LoadedModel loadModel(const std::string& path);

    void setExposure(float exp) { m_renderer.setExposure(exp); }

    void setCameraController(std::unique_ptr<ICameraController> controller);

    void createSkinnedMaterialDescriptorSet(std::shared_ptr<Material> material, SkeletalMeshComponent& skeletal);

    GameObject& loadLevel(const std::string& path);

    void setFogEnabled(bool enabled);
    void setFogSettings(const FogSettings& settings);
    void setIBLSettings(const IBLSettings& settings) { m_renderer.setIBLSettings(settings); }
    
    const FogSettings& getFogSettings() const;
    Scene& getScene()   { return m_scene; }
    Camera& getCamera() { return m_camera; }
    TimeSystem& getTime() { return m_time; }
    Input& getInput() { return m_input; }
    PhysicsSystem& getPhysics() { return m_physics; }
    AudioSystem& getAudio() { return m_audio; }
    NavMesh& getNavMesh() { return m_navMesh; }
    UISystem& getUI() { return m_ui; }

    Renderer& getRenderer() { return m_renderer; }

    void pause() { m_time.pause(); }
    void resume() { m_time.resume(); }
    void setTimeScale(float scale) { m_time.setTimeScale(scale); }

    void loadEnvironmentMap(const std::string& hdrPath) { m_renderer.loadEnvironmentMap(hdrPath); }

    GameObject& loadGltfObject(const std::string& path);
    GameObject& loadSkeletalGltfObject(const std::string& path);

    void onFramebufferResize();

    void fixedUpdate(float fixedDt);

    void setAntialiasing(AAMethod method) { m_renderer.setAAMethod(method); }

    void setCameraControlMode(bool useController) { }

    void setGodRaySettings(const GodRaySettings& settings) { m_renderer.setGodRaySettings(settings); }

    std::shared_ptr<ScreenSpaceDecal> addScreenSpaceDecal(const std::string& texturePath,
        glm::vec3 position, glm::vec3 rotationEuler, glm::vec3 size,
        const std::string& opacityPath = "", const std::string& normalPath = "", const std::string& mrPath = "");

    std::shared_ptr<Material> createDefaultMaterial();        

private:
    void updateInput(float deltaTime);
    void createMaterialDescriptorSet(std::shared_ptr<Material> material);

    GLFWwindow* m_window = nullptr;
    bool m_cleaned = false;
    float m_lastFrameTime = 0.0f;

    AudioSystem m_audio;
    TimeSystem m_time;
    Renderer m_renderer;
    ResourceManager m_resources;
    Input m_input;
    Camera m_camera;
    Scene m_scene;
    FogSettings m_fogSettings;
    UISystem m_ui;
    std::unique_ptr<ICameraController> m_cameraController;
    LightData m_lights{};
    bool m_useCameraController = true;
    PhysicsSystem m_physics;
    NavMesh m_navMesh;

};
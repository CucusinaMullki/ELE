#pragma once
#include "Vulkan/Renderer.h"
#include "Core/ResourceManager.h"
#include "Core/Input.h"
#include "Core/Camera.h"
#include "Core/AnimationLoader.h"
#include "Scene/Scene.h"
#include "Physics/PhysicsSystem.h"
#include "Physics/PhysicsDebugDraw.h"
#include "Scene/ScreenSpaceDecal.h"
#include "Core/FogSettings.h"
#include "UI/UISystem.h"
#include "Core/ICameraController.h"
#include "Nav/NavMesh.h"
#include "Core/LevelLoader.h"
#include "Scripting/Script.h"
#include "Core/LevelWriter.h"
#include "Components/SpawnPointComponent.h"
#include <filesystem>
#include "Scene/ProbeSystem.h"
#include "Core/ModelLoader.h"
#include "Core/FreeFlyCameraController.h"
#include "Components/SkeletalMeshComponent.h"
#include "RT/RTWorld.h"
#include "Core/TimeSystem.h"
#include "Audio/AudioSystem.h"
#include "Components/LightComponent.h"
#include "Core/GltfModel.h"
#include <GLFW/glfw3.h>
#include "Scene/SceneSerializer.h"
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>

struct LoadedModel
{
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<SkeletalMesh> skeletalMesh;
    std::vector<std::shared_ptr<Material>> materials;
    bool hasSkeleton = false;
};

class Engine
{
public:
    void initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();

    GameObject& addDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    GameObject& addPointLight(glm::vec3 position, glm::vec3 color, float intensity);
    GameObject& addSpotLight(glm::vec3 position, glm::vec3 direction, glm::vec3 color, float intensity,
        float innerDeg = 20.0f, float outerDeg = 30.0f, float range = 0.0f);
    GameObject& addRectLight(glm::vec3 position, glm::vec3 normal, glm::vec3 right, glm::vec2 size,
        glm::vec3 color, float intensity, bool twoSided = false, float range = 0.0f);
    void setViewportAspect(float aspect) { m_viewportAspect = aspect; }
    void setSimulationEnabled(bool enabled) { m_simulationEnabled = enabled; }
    void setGuiCallback(std::function<void()> cb) { m_guiCallback = std::move(cb);}

    std::shared_ptr<Mesh> loadMesh(const std::string& path, bool keepCpuData = false);
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
        const std::string& metallic, const std::string& roughness);
    LoadedModel loadModel(const std::string& path);
    Animation loadAnimation(const std::string& path);

    GameObject& loadLevel(const std::string& path);
    bool saveLevel(const std::string& outPath = "");
    const std::string& currentLevel() const { return m_currentLevel; }
    std::vector<GameObject*> findSpawnPoints(const std::string& type = "");

    void setExposure(float exp) { m_renderer.setExposure(exp); }

    void setCameraController(std::unique_ptr<ICameraController> controller);

    void clearScene();
    void setCameraInputEnabled(bool enabled) { m_cameraInputEnabled = enabled; }

    void createSkinnedMaterialDescriptorSet(std::shared_ptr<Material> material, SkeletalMeshComponent& skeletal);

    EngineContext componentContext();

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

    bool saveScene(const std::string& path);
    bool loadScene(const std::string& path);

    GameObject& spawnModel(const std::string& path);

    Renderer& getRenderer() { return m_renderer; }

    void setFileDropCallback(std::function<void(const std::vector<std::string>&)> cb) { m_fileDrop = std::move(cb); }

    void setTimeScale(float scale) { m_time.setTimeScale(scale); }

    void loadEnvironmentMap(const std::string& hdrPath) { m_renderer.loadEnvironmentMap(hdrPath); }

    GameObject& loadGltfObject(const std::string& path);
    GameObject& loadSkeletalGltfObject(const std::string& path);

    void setEditorMode(bool on) { if (m_rt) m_rt->setAllDynamic(on); }

    void setPaused(bool paused, bool releaseCursor = true);
    bool isPaused() const { return m_time.isPaused(); }
    void pause() { setPaused(true); }
    void resume() { setPaused(false); }

    void setCursorLocked(bool locked) { m_input.setCursorLocked(locked); }
    bool isCursorLocked() const { return m_input.isCursorLocked(); }
    void setCursorCaptured(bool captured)
    {
        if (captured == m_cursorCaptured) return;
        m_cursorCaptured = captured;
        glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    GameObject& duplicateObject(GameObject& src);

    bool collectPhysicsDebugLines(const PhysicsDebugOptions& opt, std::vector<DebugLine>& out)
    {
        return m_physics.collectDebugLines(opt, m_camera.getPosition(), out);
    }

    void dispatchFileDrop(const std::vector<std::string>& files) { if (m_fileDrop) m_fileDrop(files); }

    void setEscapeTogglesPause(bool enabled) { m_escTogglesPause = enabled; }

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
    void writeRtTextures(Material& material);
    void rebindRtTextures();
    void installRtHook();
    void gatherLights();

    std::string m_currentLevel;

    std::function<void()> m_guiCallback;

    bool m_cameraInputEnabled = true;

    GLFWwindow* m_window = nullptr;
    bool m_cleaned = false;
    float m_lastFrameTime = 0.0f;

    float m_viewportAspect = 0.0f;

    bool m_escTogglesPause = true;
    bool m_simulationEnabled = true;
    std::function<void(const std::vector<std::string>&)> m_fileDrop;

    bool m_cursorCaptured = true;
    AudioSystem m_audio;
    TimeSystem m_time;
    Renderer m_renderer;
    std::unique_ptr<RT::World> m_rt;
    std::vector<std::weak_ptr<Material>> m_rtMaterials;
    Texture m_dummyMask;
    Texture m_dummyNormal;
    std::unique_ptr<ASH::Sampler> m_rtMaskSampler;
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
    ProbeSystem m_probes;

    struct LevelAssets
    {
        std::vector<std::shared_ptr<Mesh>> meshes;
        std::vector<std::shared_ptr<Material>> materials;
    };
    LevelAssets loadLevelAssets(const LevelLoader& loader, const std::filesystem::path& levelDir);
    const LoadedModel& cachedModel(const std::string& path);
    void applySceneAsset(GameObject& obj);
    void finalizeSceneObject(GameObject& obj);

    std::unordered_map<std::string, LevelAssets> m_levelCache;
    std::unordered_map<std::string, LoadedModel> m_modelCache;
    friend class EngineSceneAssets;
};

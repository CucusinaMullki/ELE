#pragma once
#include "Vulkan/Renderer.h"
#include "Core/ResourceManager.h"
#include "Core/Input.h"
#include "Core/Camera.h"
#include "Scene/Scene.h"
#include "Scene/ScreenSpaceDecal.h"
#include "Core/FogSettings.h"
#include "Core/ICameraController.h"
#include "Core/FreeFlyCameraController.h"
#include "Core/TimeSystem.h"
#include "Core/GltfModel.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <string>


class Engine
{
public:
    void initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();

    void addDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void addPointLight(glm::vec3 position, glm::vec3 color, float intensity);

    std::shared_ptr<Mesh> loadMesh(const std::string& path);
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
        const std::string& metallic, const std::string& roughness);

    void setExposure(float exp) { m_renderer.setExposure(exp); }

    void setCameraController(std::unique_ptr<ICameraController> controller);

    void setFogEnabled(bool enabled);
    void setFogSettings(const FogSettings& settings);
    const FogSettings& getFogSettings() const;

    Scene& getScene()   { return m_scene; }
    Camera& getCamera() { return m_camera; }
    TimeSystem& getTime() { return m_time; }

    Input& getInput() { return m_input; }

    void pause() { m_time.pause(); }
    void resume() { m_time.resume(); }
    void setTimeScale(float scale) { m_time.setTimeScale(scale); }

    GameObject& loadGltfObject(const std::string& path);

    void onFramebufferResize();

    void fixedUpdate(float fixedDt);

    void setAntialiasing(AAMethod method) { m_renderer.setAAMethod(method); }

    void setGodRaySettings(const GodRaySettings& settings) { m_renderer.setGodRaySettings(settings); }

    std::shared_ptr<ScreenSpaceDecal> addScreenSpaceDecal(const std::string& texturePath,
        glm::vec3 position, glm::vec3 rotationEuler, glm::vec3 size,
        const std::string& opacityPath = "", const std::string& normalPath = "", const std::string& mrPath = "");

private:
    void updateInput(float deltaTime);

    GLFWwindow* m_window = nullptr;
    bool m_cleaned = false;
    float m_lastFrameTime = 0.0f;

    TimeSystem m_time;
    Renderer        m_renderer;
    ResourceManager m_resources;
    Input           m_input;
    Camera          m_camera;
    Scene           m_scene;
    FogSettings m_fogSettings;
    std::unique_ptr<ICameraController> m_cameraController;
    LightData m_lights{};
};
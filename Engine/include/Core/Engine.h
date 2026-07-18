#pragma once
#include "Vulkan/Renderer.h"
#include "Core/ResourceManager.h"
#include "Core/Input.h"
#include "Core/Camera.h"
#include "Scene/Scene.h"
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

    std::shared_ptr<Mesh> loadMesh(const std::string& path);
    std::shared_ptr<Material> loadMaterial(const std::string& albedo, const std::string& normal,
                                           const std::string& metallic, const std::string& roughness);

    Scene& getScene()   { return m_scene; }
    Camera& getCamera() { return m_camera; }

    GameObject& loadGltfObject(const std::string& path);

    void onFramebufferResize();

private:
    void updateInput(float deltaTime);

    GLFWwindow* m_window = nullptr;
    bool m_cleaned = false;
    float m_lastFrameTime = 0.0f;

    Renderer        m_renderer;
    ResourceManager m_resources;
    Input           m_input;
    Camera          m_camera;
    Scene           m_scene;
};
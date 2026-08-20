#include "Core/Engine.h"
#include "Core/Log.h"
#include "Nav/SimpleNavMeshBuilder.h"
#include "Components/RigidBodyComponent.h"
#include "Components/TransformComponent.h"

std::vector<unsigned char> applyMRFactors(const unsigned char* src, int w, int h,
    float metallicFactor, float roughnessFactor)
{
    std::vector<unsigned char> result(w * h * 4);
    for (int i = 0; i < w * h; i++)
    {
        result[i*4+0] = src[i*4+0];
        result[i*4+1] = (unsigned char)std::clamp((int)(src[i*4+1] * roughnessFactor), 0, 255);
        result[i*4+2] = (unsigned char)std::clamp((int)(src[i*4+2] * metallicFactor), 0, 255);
        result[i*4+3] = src[i*4+3];
    }
    return result;
}

std::vector<unsigned char> applyBaseColorFactor(const unsigned char* src, int w, int h,
    const glm::vec4& factor)
{
    std::vector<unsigned char> result(w * h * 4);
    for (int i = 0; i < w * h; i++)
    {
        result[i*4+0] = (unsigned char)std::clamp((int)(src[i*4+0] * factor.r), 0, 255);
        result[i*4+1] = (unsigned char)std::clamp((int)(src[i*4+1] * factor.g), 0, 255);
        result[i*4+2] = (unsigned char)std::clamp((int)(src[i*4+2] * factor.b), 0, 255);
        result[i*4+3] = (unsigned char)std::clamp((int)(src[i*4+3] * factor.a), 0, 255);
    }
    return result;
}

void Engine::initialize(int width, int height, const std::string& title)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    m_physics.initialize();
    m_audio.initialize();

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int, int) {
        static_cast<Engine*>(glfwGetWindowUserPointer(win))->onFramebufferResize();
    });

    m_renderer.initialize(m_window);
    m_renderer.initImGui(m_window);

    m_resources.initialize(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
        m_renderer.getCommandPool(), m_renderer.getGraphicsQueue());

    m_cameraController = std::make_unique<FreeFlyCameraController>();
    m_input.initialize(m_window);
    m_camera.initialize(glm::vec3(0.0f, 0.0f, 3.0f));
    m_camera.setAspect(m_renderer.getAspect());
}

void Engine::onFramebufferResize()
{
    m_renderer.onResize();
    m_camera.setAspect(m_renderer.getAspect());
}

std::shared_ptr<Mesh> Engine::loadMesh(const std::string& path, bool keepCpuData)
{
    return m_resources.loadMesh(path, keepCpuData);
}

std::shared_ptr<Material> Engine::loadMaterial(const std::string& albedo, const std::string& normal,
    const std::string& metallic, const std::string& roughness)
{
    auto material = m_resources.loadMaterial(albedo, normal, metallic, roughness);

    createMaterialDescriptorSet(material);

    return material;
}

void Engine::createMaterialDescriptorSet(std::shared_ptr<Material> material)
{
    material->createDescriptorSet(m_renderer.getDevice(),
        m_renderer.getDescriptorPool(), m_renderer.getDescriptorSetLayout(),
        m_renderer.getUboInfos(), m_renderer.getLightInfos(),
        m_renderer.getShadowView(), m_renderer.getShadowSampler(),
        m_renderer.getShadowAtlasView(), m_renderer.getShadowAtlasSampler(),
        m_renderer.getFramesInFlight());
}

std::shared_ptr<Material> Engine::createDefaultMaterial()
{
    static const unsigned char white[4] = { 255, 255, 255, 255 };
    static const unsigned char normalPx[4] = { 128, 128, 255, 255 };
    static const unsigned char defaultMR[4] = { 0, 128, 0, 255 };
    static const unsigned char black[4] = { 0, 0, 0, 255 };

    auto mat = std::make_shared<Material>();
    mat->loadFromPixels(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
        m_renderer.getCommandPool(), m_renderer.getGraphicsQueue(),
        white, 1, 1, normalPx, 1, 1, white, 1, 1, white, 1, 1, black, 1, 1, false);

    createMaterialDescriptorSet(mat);

    m_resources.registerMaterial(mat);
    return mat;
}

void Engine::setCameraController(std::unique_ptr<ICameraController> controller)
{
    m_cameraController = std::move(controller);
    m_cameraController->onActivate(m_camera);
}

void Engine::updateInput(float deltaTime)
{
    m_input.update();

    if (m_useCameraController)
        m_cameraController->update(m_camera, m_input, deltaTime);
        
    if (m_input.isActionJustPressed("Fire"))
    {
        LOG("Fire pressed!");
    }
}

void Engine::run()
{
    float fpsTimer = 0.0f;
    int frameCount = 0;
    
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        m_renderer.beginImGuiFrame();

        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", 1.0f / m_time.deltaTime());
        ImGui::Text("Player pos: %.2f, %.2f, %.2f", m_camera.getPosition().x, m_camera.getPosition().y, m_camera.getPosition().z);
        ImGui::End();

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (fbHeight > 0)
            m_camera.setAspect((float)fbWidth / (float)fbHeight);

        float now = static_cast<float>(glfwGetTime());
        float rawDeltaTime = now - m_lastFrameTime;
        m_lastFrameTime = now;

        m_audio.setListenerPosition(m_camera.getPosition(), m_camera.getFront(), m_camera.getUp());
        m_audio.setListenerWorldPosition(m_camera.getPosition());

        m_time.update(rawDeltaTime);

        int fixedSteps = m_time.consumeFixedSteps();
        for (int i = 0; i < fixedSteps; i++)
        {
            fixedUpdate(m_time.fixedDeltaTime());
            m_scene.fixedUpdateComponents(m_time.fixedDeltaTime());
        }

        updateInput(m_time.deltaTime());
        m_scene.updateComponents(m_time.deltaTime());

        fpsTimer += m_time.deltaTime();
        frameCount++;
        if (fpsTimer >= 1.0f)
        {
            LOG("FPS: " << frameCount);
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        m_renderer.setLights(m_lights);
        m_renderer.setTotalTime((float)m_time.totalTime());
        m_renderer.drawScene(m_scene, m_camera);
    }

    m_renderer.getDevice().waitIdle();
}

void Engine::fixedUpdate(float fixedDt)
{
    m_physics.update(fixedDt);
}

void Engine::shutdown()
{
    if (m_cleaned) return;
    m_cleaned = true;

    m_renderer.getDevice().waitIdle();

    m_resources.cleanup();
    m_renderer.cleanup();
    m_physics.cleanup();
    m_audio.cleanup();

    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

GameObject& Engine::loadGltfObject(const std::string& path)
{

    GltfModel model;
    model.load(path);

    auto mesh = std::make_shared<Mesh>();
    mesh->loadFromData(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
        model.getVertices(), model.getIndices(), model.getSubmesh());
    m_resources.registerMesh(mesh);

    static const unsigned char white[4] = { 255, 255, 255, 255 };
    static const unsigned char normalPx[4] = { 128, 128, 255, 255 };
    static const unsigned char defaultMR[4] = {0, 128, 0, 255};
    static const unsigned char black[4] = { 0, 0, 0, 255 };   // ← новое, fallback для лайтмапа

    std::vector<std::shared_ptr<Material>> materials;
    for (const auto& gm : model.getMaterials())
    {
        LOG("material: albedo=" << gm.albedoImage << " normal=" << gm.normalImage
            << " mr=" << gm.mrImage << " ao=" << gm.aoImage);
        auto getPx = [&](int imgIdx, const unsigned char* fallback, int& w, int& h) -> const unsigned char* {
            if (imgIdx >= 0 && imgIdx < (int)model.getImage().size())
            {
                const auto& img = model.getImage()[imgIdx];
                w = img.width;
                h = img.height;
                return img.pixels.data();
            }
            w = 1;
            h = 1;
            return fallback;
        };

        int aw, ah, nw, nh, mw, mh, ow, oh, lw, lh;
        const unsigned char* aPxRaw = getPx(gm.albedoImage, white, aw, ah);
        const unsigned char* nPx = getPx(gm.normalImage, normalPx, nw, nh);
        const unsigned char* mPxRaw = getPx(gm.mrImage, defaultMR, mw, mh);
        const unsigned char* oPx = getPx(gm.aoImage, white, ow, oh);
        const unsigned char* lPx = getPx(gm.lightmapImage, black, lw, lh);

        std::vector<unsigned char> aAdjusted = applyBaseColorFactor(aPxRaw, aw, ah, gm.baseColorFactor);
        const unsigned char* aPx = aAdjusted.data();

        std::vector<unsigned char> mrAdjusted = applyMRFactors(mPxRaw, mw, mh, gm.metallicFactor, gm.roughnessFactor);
        const unsigned char* mPx = mrAdjusted.data();

        auto mat = std::make_shared<Material>();
        mat->loadFromPixels(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
            m_renderer.getCommandPool(), m_renderer.getGraphicsQueue(),
            aPx, aw, ah, nPx, nw, nh, mPx, mw, mh, oPx, ow, oh,
            lPx, lw, lh, gm.hasLightmap);

        createMaterialDescriptorSet(mat);

        m_resources.registerMaterial(mat);
        materials.push_back(mat);
    }

    for (const auto& gl : model.getLights())
    {
        switch (gl.type)
        {
            case 0:
                addDirectionalLight(gl.direction, gl.color, gl.intensity / 683.0f);
                break;
            case 1:
                addPointLight(gl.position, gl.color, gl.intensity / 683.0f);
                break;
            default:
                break;
        }
    }

    for (const auto& gc : model.getColliders())
    {
        auto& obj = m_scene.addObject();
        obj.transform.position = gc.position;
        obj.transform.rotation = gc.rotationEuler;

        RigidBodyComponent::Shape shape = (gc.shape == "sphere")
            ? RigidBodyComponent::Shape::Sphere : RigidBodyComponent::Shape::Box;
        RigidBodyComponent::Motion motion = (gc.motion == "dynamic")
            ? RigidBodyComponent::Motion::Dynamic : RigidBodyComponent::Motion::Static;

        obj.addComponent<TransformComponent>();
        obj.addComponent<RigidBodyComponent>(m_physics, shape, motion, nullptr, gc.size, gc.mass);
    }

    for (const auto& gd : model.getDecals())
    {
        auto decal = addScreenSpaceDecal(gd.texturePath, gd.position, gd.rotationEuler, gd.size,
            gd.opacityPath, gd.normalPath, gd.mrPath);
        decal->metallic  = gd.metallic;
        decal->roughness = gd.roughness;
    }

    for (const auto& ge : model.getEntities())
    {
        auto& obj = m_scene.addObject();
        obj.name = ge.name;
        obj.entityType = ge.entityType;
        obj.transform.position = ge.position;
        obj.transform.rotation = ge.rotationEuler;
        obj.transform.scale = ge.scale;
        obj.addComponent<TransformComponent>();
    }

    for (const auto& nv : model.getNavVolumes())
    {

        LOG("Building navmesh: volume size=(" << nv.size.x << "," << nv.size.y << "," << nv.size.z 
            << ") cellSize=" << nv.cellSize);
        glm::vec3 volumeMin = nv.position - nv.size;
        glm::vec3 volumeMax = nv.position + nv.size;

        SimpleNavMeshBuilder builder;
        m_navMesh = builder.build(m_physics, volumeMin, volumeMax, nv.cellSize);
    }

    if (materials.empty())
    {
        auto mat = std::make_shared<Material>();
        mat->loadFromPixels(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
            m_renderer.getCommandPool(), m_renderer.getGraphicsQueue(),
            white, 1, 1, normalPx, 1, 1, white, 1, 1, white, 1, 1, black, 1, 1, false);
        createMaterialDescriptorSet(mat);
        m_resources.registerMaterial(mat);
        materials.push_back(mat);
    }

    auto& obj = m_scene.addObject();
    obj.mesh = mesh;
    obj.materials = materials;
    return obj;
}

void Engine::setFogEnabled(bool enabled)
{
    m_fogSettings.enabled = enabled;
    m_renderer.setFogSettings(m_fogSettings);
}

void Engine::setFogSettings(const FogSettings& settings)
{
    m_fogSettings = settings;
    m_renderer.setFogSettings(m_fogSettings);
}

const FogSettings& Engine::getFogSettings() const
{
    return m_fogSettings;
}

void Engine::addDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
    int i = m_lights.count.x;
    if (i >= MAX_LIGHTS) return;
    m_lights.lights[i].position = glm::vec4(direction, 0.0f);
    m_lights.lights[i].color = glm::vec4(color, intensity);
    m_lights.count.x++;
}

void Engine::addPointLight(glm::vec3 position, glm::vec3 color, float intensity)
{
    int i = m_lights.count.x;
    if (i >= MAX_LIGHTS) return;
    m_lights.lights[i].position = glm::vec4(position, 1.0f); 
    m_lights.lights[i].color = glm::vec4(color, intensity);
    LOG("addPointLight called with y=" << position.y);
    m_lights.count.x++;
}

std::shared_ptr<ScreenSpaceDecal> Engine::addScreenSpaceDecal(const std::string& texturePath,
    glm::vec3 position, glm::vec3 rotationEuler, glm::vec3 size,
    const std::string& opacityPath, const std::string& normalPath, const std::string& mrPath)
{
    LOG("addScreenSpaceDecal called!");

    auto decal = std::make_shared<ScreenSpaceDecal>();
    decal->position = position;
    decal->rotationEuler = rotationEuler;
    decal->size = size;

    decal->loadTexture(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
        m_renderer.getCommandPool(), m_renderer.getGraphicsQueue(), texturePath, opacityPath, normalPath, mrPath);

    LOG("using layout ptr=" << (void*)(VkDescriptorSetLayout)m_renderer.getDecalDescriptorSetLayout());

    decal->createDescriptorSet(m_renderer.getDevice(), m_renderer.getDecalDescriptorPool(),
        m_renderer.getDecalDescriptorSetLayout(), m_renderer.getDepthView(), m_renderer.getDepthSampler(),
        m_renderer.getDecalCameraBuffer(), m_renderer.getLightBuffer(), m_renderer.getFramesInFlight());

    m_renderer.addScreenSpaceDecal(decal);
    return decal;
}
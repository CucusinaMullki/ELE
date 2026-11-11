#include "Core/Engine.h"
#include "Core/LevelLoader.h"
#include <stb_image.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cstdio>
#include "Core/Log.h"
#include "Scene/PropertyIO.h"
#include "Core/Path.h"
#include "RT/Blas.h"
#include <glm/gtc/type_ptr.hpp>
#include "RT/RTContext.h"
#include "Nav/SimpleNavMeshBuilder.h"
#include "Scene/SkeletalGltfLoader.h"
#include "Components/TriggerComponent.h"
#include "Components/RigidBodyComponent.h"
#include "Components/TransformComponent.h"

namespace { Engine* g_dropEngine = nullptr; }

class EngineSceneAssets final : public ISceneAssets
{
public:
    explicit EngineSceneAssets(Engine& e) : m_e(e) {}
    void applyAsset(GameObject& o) override { m_e.applySceneAsset(o); }
    void finalize(GameObject& o) override { m_e.finalizeSceneObject(o); }
    bool loadLevel(const std::string& path) override
    {
        try { m_e.loadLevel(path); return true; }
        catch (const std::exception& ex) { LOG("loadScene: level: " << ex.what()); return false; }
    }
private:
    Engine& m_e;
};

const LoadedModel& Engine::cachedModel(const std::string& path)
{
    auto it = m_modelCache.find(path);
    if (it == m_modelCache.end())
        it = m_modelCache.emplace(path, loadModel(path)).first;
    return it->second;
}

void Engine::clearScene()
{
    m_renderer.getDevice()->waitIdle();
    m_scene.clear();
    if (m_rt) m_rt->sync(m_scene);
}

void Engine::applySceneAsset(GameObject& o)
{
    switch (o.asset.kind)
    {
    case AssetRef::Kind::LevelMesh:
    {
        auto it = m_levelCache.find(o.asset.path);
        if (it == m_levelCache.end())
        {
            const std::filesystem::path file = Paths::assetDir() / o.asset.path;
            LevelLoader loader;
            loader.load(o.asset.path);
            it = m_levelCache.emplace(o.asset.path, loadLevelAssets(loader, file.parent_path())).first;
        }
        const LevelAssets& la = it->second;
        if (o.asset.index < 0 || o.asset.index >= (int)la.meshes.size())
        {
            LOG("loadScene: '" << o.name << "' bad mesh index " << o.asset.index);
            return;
        }
        o.mesh = la.meshes[o.asset.index];
        for (const auto& sm : o.mesh->getSubmeshes())
            if (sm.materialIndex >= 0 && sm.materialIndex < (int)la.materials.size())
                o.setMaterial(la.materials[sm.materialIndex]);
        break;
    }
    case AssetRef::Kind::Model:
    {
        const LoadedModel& m = cachedModel(o.asset.path);
        o.mesh      = m.mesh;
        o.materials = m.materials;
        break;
    }
    default: break;
    }
}

void Engine::finalizeSceneObject(GameObject& o)
{
    if (o.asset.kind != AssetRef::Kind::Skeletal) return;

    const LoadedModel& m = cachedModel(o.asset.path);
    auto* sk = o.getComponent<SkeletalMeshComponent>();
    if (!sk)
        sk = &o.addComponent<SkeletalMeshComponent>(m_renderer.getDevice(), m_renderer.getFramesInFlight());

    sk->setSkeletalMesh(m.skeletalMesh);
    o.materials = m.materials;
    for (auto& mat : o.materials)
        createSkinnedMaterialDescriptorSet(mat, *sk);
}

bool Engine::saveScene(const std::string& path)
{
    return SceneSerializer::save(m_scene, (Paths::assetDir() / path).string(), m_currentLevel);
}

GameObject& Engine::addDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
    auto& obj = m_scene.addObject();
    obj.name = "DirectionalLight";
    setLightRotation(obj.transform, direction);
    auto& lc = obj.addComponent<LightComponent>();
    lc.type = LightType::Directional;
    lc.color = color;
    lc.intensity = intensity;
    return obj;
}

GameObject& Engine::addPointLight(glm::vec3 position, glm::vec3 color, float intensity)
{
    auto& obj = m_scene.addObject();
    obj.name = "PointLight";
    obj.transform.position = position;
    auto& lc = obj.addComponent<LightComponent>();
    lc.type = LightType::Point;
    lc.color = color;
    lc.intensity = intensity;
    return obj;
}

GameObject& Engine::addSpotLight(glm::vec3 position, glm::vec3 direction, glm::vec3 color, float intensity,
    float innerDeg, float outerDeg, float range)
{
    auto& obj = m_scene.addObject();
    obj.name = "SpotLight";
    obj.transform.position = position;
    setLightRotation(obj.transform, direction);
    auto& lc = obj.addComponent<LightComponent>();
    lc.type = LightType::Spot;
    lc.color = color;
    lc.intensity = intensity;
    lc.innerAngle = innerDeg;
    lc.outerAngle = outerDeg;
    lc.range = range;
    return obj;
}

GameObject& Engine::addRectLight(glm::vec3 position, glm::vec3 normal, glm::vec3 right, glm::vec2 size,
    glm::vec3 color, float intensity, bool twoSided, float range)
{
    auto& obj = m_scene.addObject();
    obj.name = "RectLight";
    obj.transform.position = position;
    setLightRotation(obj.transform, normal, right);
    auto& lc = obj.addComponent<LightComponent>();
    lc.type = LightType::Rect;
    lc.color = color;
    lc.intensity = intensity;
    lc.size = size;
    lc.twoSided = twoSided;
    lc.range = range;
    return obj;
}

void Engine::gatherLights()
{
    m_lights = {};
    for (const auto& obj : m_scene.getObjects())
    {
        auto* lc = obj->getComponent<LightComponent>();
        if (!lc || !lc->enabled) continue;
        if (m_lights.count.x >= MAX_LIGHTS) break;
        m_lights.lights[m_lights.count.x++] = lc->toGpu(obj->transform);
    }
}

GameObject& Engine::duplicateObject(GameObject& src)
{
    BinaryWriter w;
    SceneSerializer::writeObject(w, src);
    BinaryReader r(w.data().data(), w.size());

    EngineContext ctx = componentContext();
    EngineSceneAssets assets(*this);
    GameObject* copy = SceneSerializer::readObject(r, m_scene, ctx, assets);
    if (!copy) throw std::runtime_error("duplicate failed: " + src.name);

    copy->name = src.name + " (copy)";
    copy->fromLevel = src.fromLevel;
    copy->levelProperties = src.levelProperties;

    if (m_rt) { m_scene.flush(); m_rt->sync(m_scene); }
    return *copy;
}

EngineContext Engine::componentContext()
{
    EngineContext c;
    c.device = m_renderer.getDevice();
    c.physics = &m_physics;
    c.framesInFlight = m_renderer.getFramesInFlight();
    c.audio = &m_audio;
    c.input = &m_input;
    c.time = &m_time;
    c.camera = &m_camera;
    return c;
}

bool Engine::loadScene(const std::string& path)
{
    EngineContext ctx = componentContext();
    EngineSceneAssets assets(*this);
    const bool ok = SceneSerializer::load(m_scene, (Paths::assetDir() / path).string(), ctx, assets);

    if (m_rt)
    {
        m_scene.flush();
        m_rt->sync(m_scene);
        rebindRtTextures();
    }
    return ok;
}

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
    registerEngineComponents(ComponentRegistry::instance());
    m_audio.initialize();
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int, int) {
        static_cast<Engine*>(glfwGetWindowUserPointer(win))->onFramebufferResize();
    });
    g_dropEngine = this;
    glfwSetDropCallback(m_window, [](GLFWwindow*, int count, const char** paths) {
        if (g_dropEngine && g_dropEngine->m_fileDrop)
            g_dropEngine->m_fileDrop(std::vector<std::string>(paths, paths + count));
    });
    m_renderer.initialize(m_window);
    m_renderer.initImGui(m_window);
    m_probes.initialize(m_renderer.getDevice());
    m_renderer.getParticles().setProbeBuffers(m_probes.getDataBuffer(), m_probes.getInfoBuffer());
    m_resources.initialize(m_renderer.getDevice());
    m_cameraController = std::make_unique<FreeFlyCameraController>();
    m_input.initialize(m_window);
    m_camera.initialize(glm::vec3(0.0f, 0.0f, 3.0f));
    m_camera.setAspect(m_renderer.getAspect());
    {
        auto rt = std::make_unique<RT::World>();
        const uint32_t frames = m_renderer.getFramesInFlight();
        int fw = 0, fh = 0;
        glfwGetFramebufferSize(m_window, &fw, &fh);

        if (rt->init(m_renderer.getDevice(), frames, (uint32_t)fw, (uint32_t)fh))
        {
            m_rt = std::move(rt);
            std::cout << "RT: enabled and initialized successfully\n";
        }
        else
        {
            std::cout << "RT: init FAILED. Check console logs above!\n";
        }
    }
    static const unsigned char white[4] = { 255, 255, 255, 255 };
    m_dummyMask.initializeFromMemory(m_renderer.getDevice(), white, 1, 1, false);

    static const unsigned char flatN[4] = { 128, 128, 255, 255 };
    m_dummyNormal.initializeFromMemory(m_renderer.getDevice(), flatN, 1, 1, false);

    ASH::SamplerDesc rtSd{};
    rtSd.magFilter = ASH::Filter::Nearest;
    rtSd.minFilter = ASH::Filter::Nearest;
    rtSd.addressModeU = ASH::AddressMode::ClampToEdge;
    rtSd.addressModeV = ASH::AddressMode::ClampToEdge;
    rtSd.addressModeW = ASH::AddressMode::ClampToEdge;
    rtSd.compareEnable = false;
    m_rtMaskSampler = m_renderer.getDevice()->createSampler(rtSd);

    if (m_rt)
    {
        const uint32_t frames = m_renderer.getFramesInFlight();

        m_rt->setDefaultTexture(m_dummyMask.getTexture(), m_dummyNormal.getTexture());

        for (uint32_t i = 0; i < frames; ++i) {
            m_rt->bindFrameBuffers(i,
                m_renderer.getUniformBuffer().getBuffer(i),
                m_renderer.getLightBuffer().getBuffer(i));
        }

        m_rt->sync(m_scene);
        installRtHook();
    }

    Script::setEngine(this);
}

void Engine::installRtHook()
{
    m_renderer.setPreMainPassHook([this](ASH::CommandBuffer* cmd, uint32_t frame) {
        if (!m_rt) return;
        m_rt->sync(m_scene);
        m_rt->updateSkinned(m_scene, cmd, frame);
        m_rt->updateDynamic(m_scene, cmd, frame);
        m_rt->recordAll(cmd, frame);

        const glm::mat4 v = m_camera.getViewMatrix();
        const glm::mat4 p = m_camera.getProjectionMatrix();
        m_rt->recordDenoise(cmd, frame, glm::value_ptr(v), glm::value_ptr(p));
    });
}

void Engine::writeRtTextures(Material& material)
{
    const uint32_t frames = m_renderer.getFramesInFlight();
    ASH::Texture* dummy = m_dummyMask.getTexture();
    ASH::Sampler* s = m_rtMaskSampler.get();
    auto pick = [&](ASH::Texture* t) { return t ? t : dummy; };

    for (uint32_t i = 0; i < frames; ++i)
    {
        material.writeRtMask      (i, pick(m_rt ? m_rt->shadowImage(i)     : nullptr), s);
        material.writeRtReflection(i, pick(m_rt ? m_rt->reflectionImage(i) : nullptr), s);
        material.writeRtAo        (i, pick(m_rt ? m_rt->aoImage(i)         : nullptr), s);
        material.writeRtGi        (i, pick(m_rt ? m_rt->giImage(i)         : nullptr), s);
    }
}

void Engine::rebindRtTextures()
{
    auto& list = m_rtMaterials;
    list.erase(std::remove_if(list.begin(), list.end(),
        [](const std::weak_ptr<Material>& w) { return w.expired(); }), list.end());
    for (auto& w : list)
        if (auto m = w.lock()) writeRtTextures(*m);
}

void Engine::setPaused(bool paused, bool releaseCursor)
{
    if (paused) m_time.pause(); else m_time.resume();
    if (releaseCursor) m_input.setCursorLocked(!paused);
}



void Engine::onFramebufferResize()
{
    m_renderer.onResize();
    m_camera.setAspect(m_renderer.getAspect());

    if (m_rt)
    {
        int fw = 0, fh = 0;
        glfwGetFramebufferSize(m_window, &fw, &fh);
        if (fw > 0 && fh > 0)
        {
            m_rt->resize((uint32_t)fw, (uint32_t)fh);
            rebindRtTextures();
        }

        const uint32_t frames = m_renderer.getFramesInFlight();
        for (uint32_t i = 0; i < frames; ++i)
        {
            m_rt->bindFrameBuffers(i,
                m_renderer.getUniformBuffer().getBuffer(i),
                m_renderer.getLightBuffer().getBuffer(i));
        }
    }
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
    m_renderer.getDevice()->waitIdle();
    material->createDescriptorSet(m_renderer.getDevice(),
        m_renderer.getDescriptorSetLayout(),
        m_renderer.getUboBuffers(), m_renderer.getLightBuffers(),
        m_renderer.getShadowView(), m_renderer.getShadowSampler(),
        m_renderer.getShadowAtlasView(), m_renderer.getShadowAtlasSampler(),
        m_renderer.getIrradianceView(), m_renderer.getIrradianceSampler(),
        m_renderer.getVoxelAtlasTexture(), m_renderer.getVoxelAtlasSampler(), m_renderer.getVoxelPageTableBuffer(),
        m_renderer.getVoxelGridInfoBuffer(),
        m_probes.getDataBuffer(), m_probes.getInfoBuffer(),
        m_renderer.getFramesInFlight());

    writeRtTextures(*material);
    m_rtMaterials.push_back(material);
    if (m_rt) m_rt->registerMaterial(material.get(), material->albedoTexture(), material->getNormalTexture());
}

std::shared_ptr<Material> Engine::createDefaultMaterial()
{
    static const unsigned char white[4] = { 255, 255, 255, 255 };
    static const unsigned char normalPx[4] = { 128, 128, 255, 255 };
    static const unsigned char defaultMR[4] = { 0, 128, 0, 255 };
    static const unsigned char black[4] = { 0, 0, 0, 255 };

    auto mat = std::make_shared<Material>();
    mat->loadFromPixels(m_renderer.getDevice(),
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

    if (m_escTogglesPause && m_input.isKeyJustPressed(Key::Escape))
        setPaused(!m_time.isPaused());

    if (m_useCameraController && !m_time.isPaused())
        m_cameraController->update(m_camera, m_input, deltaTime);

}

void Engine::createSkinnedMaterialDescriptorSet(std::shared_ptr<Material> material, SkeletalMeshComponent& skeletal)
{
    std::vector<ASH::Buffer*> boneBuffers;
    for (uint32_t i = 0; i < m_renderer.getFramesInFlight(); i++)
        boneBuffers.push_back(skeletal.getBoneBuffer(i));

    material->createSkinnedDescriptorSet(m_renderer.getDevice(),
        m_renderer.getSkinnedDescriptorSetLayout(),
        m_renderer.getUboBuffers(), m_renderer.getLightBuffers(),
        m_renderer.getShadowView(), m_renderer.getShadowSampler(),
        m_renderer.getShadowAtlasView(), m_renderer.getShadowAtlasSampler(),
        boneBuffers,
        m_renderer.getIrradianceView(), m_renderer.getIrradianceSampler(),
        m_probes.getDataBuffer(), m_probes.getInfoBuffer(),
        m_renderer.getFramesInFlight());

    writeRtTextures(*material);
    m_rtMaterials.push_back(material);
    if (m_rt) m_rt->registerMaterial(material.get(), material->albedoTexture(), material->getNormalTexture());
}

void Engine::run()
{
    float fpsTimer = 0.0f;
    int frameCount = 0;

    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        m_renderer.beginImGuiFrame();
        m_ui.draw();
        if (m_guiCallback) m_guiCallback();

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (m_viewportAspect > 0.0f)
            m_camera.setAspect(m_viewportAspect);
        else if (fbHeight > 0)
            m_camera.setAspect((float)fbWidth / (float)fbHeight);

        float now = static_cast<float>(glfwGetTime());
        float rawDeltaTime = now - m_lastFrameTime;
        m_lastFrameTime = now;

        m_audio.setListenerPosition(m_camera.getPosition(), m_camera.getFront(), m_camera.getUp());
        m_audio.setListenerWorldPosition(m_camera.getPosition());

        m_time.update(rawDeltaTime);

        int fixedSteps = m_time.isPaused() ? 0 : m_time.consumeFixedSteps();
        if (!m_simulationEnabled) fixedSteps = 0;
        for (int i = 0; i < fixedSteps; i++)
        {
            fixedUpdate(m_time.fixedDeltaTime());
            m_scene.fixedUpdateComponents(m_time.fixedDeltaTime());
        }

        if (m_cameraInputEnabled)
            updateInput(m_time.deltaTime());
        if (m_simulationEnabled)
            m_scene.updateComponents(m_time.deltaTime());
        else
            m_scene.flush();

        fpsTimer += m_time.deltaTime();
        frameCount++;
        if (fpsTimer >= 1.0f)
        {
            LOG("FPS: " << frameCount);
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        gatherLights();
        m_renderer.setLights(m_lights);
        m_renderer.setTotalTime((float)m_time.totalTime());
        m_renderer.drawScene(m_scene, m_camera);
    }

    m_renderer.getDevice()->waitIdle();
}

GameObject& Engine::spawnModel(const std::string& path)
{
    const LoadedModel& m = cachedModel(path);
    auto& obj = m_scene.addObject();
    obj.name = std::filesystem::path(path).stem().string();
    obj.asset = { m.hasSkeleton ? AssetRef::Kind::Skeletal : AssetRef::Kind::Model, path };

    if (m.hasSkeleton) finalizeSceneObject(obj);
    else applySceneAsset(obj);

    if (m_rt)
    {
        m_scene.flush();
        m_rt->sync(m_scene);
        rebindRtTextures();
    }
    return obj;
}

void Engine::fixedUpdate(float fixedDt)
{
    m_physics.update(fixedDt);
}

void Engine::shutdown()
{
    g_dropEngine = nullptr;
    if (m_window) glfwSetDropCallback(m_window, nullptr);
    if (m_cleaned) return;
    m_cleaned = true;

    if (m_rt)
    {
        m_rt->shutdown();
        m_rt.reset();
    }

    m_modelCache.clear();
    m_levelCache.clear();
    m_renderer.setPreMainPassHook(nullptr);
    m_rtMaterials.clear();
    m_rtMaskSampler.reset();
    m_dummyMask.cleanup();
    m_dummyNormal.cleanup();
    m_renderer.getDevice()->waitIdle();
    m_probes.shutdown();
    m_scene.clear();
    m_resources.cleanup();
    m_renderer.cleanup();
    m_physics.cleanup();
    m_audio.cleanup();

    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

GameObject& Engine::loadSkeletalGltfObject(const std::string& path)
{
    SkeletalGltfResult loaded = SkeletalGltfLoader::load(path);

    auto skeletalMesh = std::make_shared<SkeletalMesh>();

    Submesh sm{};
    sm.firstIndex = 0;
    sm.indexCount = (uint32_t)loaded.indices.size();
    sm.materialIndex = 0;

    skeletalMesh->loadFromData(m_renderer.getDevice(),
        loaded.vertices, loaded.indices, { sm });
    skeletalMesh->setSkeleton(loaded.skeleton);

    auto& obj = m_scene.addObject();

    for (auto& anim : loaded.animations)
        obj.animations[anim->name] = anim;

    auto& skeletalComp = obj.addComponent<SkeletalMeshComponent>(
        m_renderer.getDevice(), m_renderer.getFramesInFlight());
    skeletalComp.setSkeletalMesh(skeletalMesh);

    if (!loaded.animations.empty())
        skeletalComp.play(loaded.animations[0], true);

    LOG("loadSkeletalGltfObject: loaded '" << path << "' with " << obj.animations.size() << " animations:");

    return obj;
}

GameObject& Engine::loadGltfObject(const std::string& path)
{

    GltfModel model;
    model.load(path);

    auto mesh = std::make_shared<Mesh>();
    mesh->loadFromData(m_renderer.getDevice(), model.getVertices(), model.getIndices(), model.getSubmesh());
    m_resources.registerMesh(mesh);

    static const unsigned char white[4] = { 255, 255, 255, 255 };
    static const unsigned char normalPx[4] = { 128, 128, 255, 255 };
    static const unsigned char defaultMR[4] = {0, 128, 0, 255};
    static const unsigned char black[4] = { 0, 0, 0, 255 };

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
        mat->loadFromPixels(m_renderer.getDevice(),
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
        mat->loadFromPixels(m_renderer.getDevice(),
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

GameObject& Engine::loadLevel(const std::string& path)
{
    std::filesystem::path levelFile(path);
    if (levelFile.is_relative() && std::filesystem::exists(Paths::assetDir() / levelFile))
        levelFile = Paths::assetDir() / levelFile;
    const std::filesystem::path levelDir = levelFile.parent_path();

    LevelLoader loader;
    loader.load(levelFile.string());

    std::filesystem::path probePath = levelFile;
    probePath.replace_extension(".eprobes");
    m_probes.load(probePath.string());

    {
        std::filesystem::path partPath = levelFile;
        partPath.replace_extension(".eparticles");
        if (std::filesystem::exists(partPath))
            m_renderer.getParticles().load(partPath.string());
    }

    const std::string levelRel = std::filesystem::relative(levelFile, Paths::assetDir()).generic_string();
    m_currentLevel = levelRel;

    LevelAssets& la = m_levelCache[levelRel] = loadLevelAssets(loader, levelDir);
    auto& meshes = la.meshes;
    auto& materials = la.materials;

    GameObject* rootObject = nullptr;

    for (const auto& levelObj : loader.getObjects())
    {
        auto& obj = m_scene.addObject();
        obj.name = levelObj.name;
        obj.fromLevel = true;
        obj.levelProperties = levelObj.customProperties;
        if (levelObj.meshIndex >= 0)
            obj.asset = { AssetRef::Kind::LevelMesh, levelRel, levelObj.meshIndex };

        if (levelObj.hasTrs)
            obj.transform = levelObj.trs;
        else if (levelObj.meshIndex < 0)
            obj.transform.position = glm::vec3(levelObj.transform[3]);

        auto entityTypeIt = levelObj.customProperties.find("entity_type");
        if (entityTypeIt != levelObj.customProperties.end())
            obj.entityType = entityTypeIt->second;

        if (!rootObject) rootObject = &obj;

        if (levelObj.meshIndex >= 0 && levelObj.meshIndex < (int)meshes.size())
        {
            obj.mesh = meshes[levelObj.meshIndex];

            for (const auto& sm : obj.mesh->getSubmeshes())
            {
                if (sm.materialIndex >= 0 && sm.materialIndex < (int)materials.size())
                    obj.setMaterial(materials[sm.materialIndex]);
            }
        }

        auto it = levelObj.customProperties.find("collision_shape");
        if (it != levelObj.customProperties.end())
        {
            std::string shape = it->second;
            std::string motion = levelObj.customProperties.count("collision_motion")
                ? levelObj.customProperties.at("collision_motion") : "static";
            float mass = levelObj.customProperties.count("collision_mass")
                ? std::stof(levelObj.customProperties.at("collision_mass")) : 1.0f;

            RigidBodyComponent::Shape shapeEnum = (shape == "sphere")
                ? RigidBodyComponent::Shape::Sphere : RigidBodyComponent::Shape::Box;
            RigidBodyComponent::Motion motionEnum = (motion == "dynamic")
                ? RigidBodyComponent::Motion::Dynamic
                : (motion == "kinematic" ? RigidBodyComponent::Motion::Kinematic : RigidBodyComponent::Motion::Static);

            glm::vec3 halfExtent(0.5f);

            auto sizeIt = levelObj.customProperties.find("collision_size");
            if (sizeIt != levelObj.customProperties.end())
            {
                float x, y, z;
                if (sscanf(sizeIt->second.c_str(), "%f,%f,%f", &x, &y, &z) == 3)
                    halfExtent = glm::vec3(x, y, z);
            }
            else if (obj.mesh && !obj.mesh->getSubmeshes().empty())
            {
                const auto& sm = obj.mesh->getSubmeshes()[0];
                halfExtent = (sm.boundsMax - sm.boundsMin) * 0.5f;
            }

            glm::vec3 pos = glm::vec3(levelObj.transform[3]);
            obj.addComponent<TransformComponent>().setPosition(pos);
            obj.addComponent<RigidBodyComponent>(m_physics, shapeEnum, motionEnum, nullptr, halfExtent, mass);
        }

        auto typeIt = levelObj.customProperties.find("ele_type");
        if (typeIt != levelObj.customProperties.end())
        {
            if (typeIt->second == "trigger")
            {
                glm::vec3 halfExtent(0.5f);
                obj.addComponent<TriggerComponent>(m_physics, halfExtent);
            }
        }
    }

    LOG("LevelLoader: lightCount=" << loader.getLights().size());
    for (const auto& light : loader.getLights())
    {
        if (light.hasComponent)
        {
            auto& obj = m_scene.addObject();
            obj.name = light.name;
            obj.fromLevel = true;
            obj.transform = light.trs;
            auto& lc = obj.addComponent<LightComponent>();
            BinaryPropertyReader pr(BinaryReader(light.componentData.data(), light.componentData.size()));
            lc.visit(pr);
            if (!rootObject) rootObject = &obj;
            continue;
        }

        GameObject* lo = nullptr;
        switch (light.type)
        {
            case 0: lo = &addDirectionalLight(light.direction, light.color, light.intensity * 0.1f); break;
            case 1: lo = &addPointLight(light.position, light.color, light.intensity * 0.1f); break;
            default: break;
        }
        if (lo)
        {
            lo->fromLevel = true;
            if (!rootObject) rootObject = lo;
        }
    }

    for (const auto& sp : loader.getSpawns())
    {
        auto& obj = m_scene.addObject();
        obj.name = sp.name;
        obj.fromLevel = true;
        obj.transform = sp.trs;
        obj.levelProperties = sp.properties;
        obj.addComponent<SpawnPointComponent>(sp.type);
        if (!rootObject) rootObject = &obj;
    }

    if (m_rt)
    {
        m_scene.flush();
        m_rt->sync(m_scene);

        LOG("RT: TLAS instances = " << m_rt->instanceCount() << ", geometries = " << m_rt->geometryCount() << "\n");
    }

    if (!rootObject)
        throw std::runtime_error("loadLevel: level is empty: " + levelRel);
    return *rootObject;
}

bool Engine::saveLevel(const std::string& outPath)
{
    if (m_currentLevel.empty())
    {
        LOG("saveLevel: no level loaded");
        return false;
    }
    const std::string out = outPath.empty() ? m_currentLevel : outPath;
    if (!LevelWriter::save((Paths::assetDir() / m_currentLevel).string(),
        (Paths::assetDir() / out).string(), m_scene))
        return false;
    m_currentLevel = std::filesystem::path(out).generic_string();
    return true;
}

std::vector<GameObject*> Engine::findSpawnPoints(const std::string& type)
{
    std::vector<GameObject*> out;
    for (const auto& o : m_scene.getObjects())
        if (auto* sp = o->getComponent<SpawnPointComponent>())
            if (type.empty() || sp->spawnType == type)
                out.push_back(o.get());
    return out;
}

Engine::LevelAssets Engine::loadLevelAssets(const LevelLoader& loader, const std::filesystem::path& levelDir)
{
    LevelAssets out;

    for (const auto& levelMesh : loader.getMeshes())
    {
        std::vector<Submesh> submeshes;
        for (const auto& sm : levelMesh.submeshes)
        {
            Submesh submesh{};
            submesh.materialIndex = sm.materialIndex;
            submesh.firstIndex = sm.indexOffset;
            submesh.indexCount = sm.indexCount;
            submesh.boundsMin = sm.boundsMin;
            submesh.boundsMax = sm.boundsMax;
            submeshes.push_back(submesh);
        }
        auto mesh = std::make_shared<Mesh>();

        mesh->loadFromData(m_renderer.getDevice(), levelMesh.vertices, levelMesh.indices, submeshes, true);
        if (m_rt) m_rt->addMesh(mesh.get(), levelMesh.vertices, levelMesh.indices, submeshes);
        out.meshes.push_back(mesh);
    }

    for (const auto& levelMat : loader.getMaterials())
    {
        auto mat = std::make_shared<Material>();
        mat->loadFromFiles(m_renderer.getDevice(),
            Paths::resolveAsset(levelMat.albedoPath, levelDir).string(),
            Paths::resolveAsset(levelMat.normalPath, levelDir).string(),
            Paths::resolveAsset(levelMat.metallicRoughnessPath, levelDir).string());
        createMaterialDescriptorSet(mat);
        m_resources.registerMaterial(mat);
        out.materials.push_back(mat);
    }
    return out;
}

Animation Engine::loadAnimation(const std::string& path)
{
    return AnimationLoader::load(path);
}

LoadedModel Engine::loadModel(const std::string& path)
{
    ModelLoader loader;
    loader.load(path);

    std::vector<Submesh> submeshes;
    for (const auto& sm : loader.getSubmeshes())
    {
        Submesh submesh{};
        submesh.materialIndex = sm.materialIndex;
        submesh.firstIndex = sm.indexOffset;
        submesh.indexCount = sm.indexCount;
        submesh.boundsMin = sm.boundsMin;
        submesh.boundsMax = sm.boundsMax;
        submeshes.push_back(submesh);
    }

    LoadedModel result{};
    result.hasSkeleton = loader.hasSkeleton();

    if (result.hasSkeleton)
    {
        auto skeletalMesh = std::make_shared<SkeletalMesh>();
        skeletalMesh->loadFromData(m_renderer.getDevice(), loader.getSkinnedVertices(), loader.getIndices(), submeshes);

        auto skeleton = std::make_shared<Skeleton>(loader.buildSkeleton());
        skeletalMesh->setSkeleton(skeleton);

        result.skeletalMesh = skeletalMesh;
    }
    else
    {
        auto mesh = std::make_shared<Mesh>();
        mesh->loadFromData(m_renderer.getDevice(), loader.getVertices(), loader.getIndices(), submeshes, true);
        m_resources.registerMesh(mesh);
        if (m_rt) m_rt->addMesh(mesh.get(), loader.getVertices(), loader.getIndices(), submeshes);
        result.mesh = mesh;
    }

    static const unsigned char white[4] = { 255, 255, 255, 255 };
    static const unsigned char normalPx[4] = { 128, 128, 255, 255 };
    static const unsigned char defaultMR[4] = { 0, 128, 0, 255 };
    static const unsigned char black[4] = { 0, 0, 0, 255 };

    auto fullPath = [](const std::string& p) { return p.empty() ? p : (Paths::assetDir() / p).string(); };

    auto loadPixels = [](const std::string& path, int& w, int& h) -> unsigned char*
    {
        if (path.empty()) return nullptr;
        unsigned char* px = stbi_load(path.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
        if (!px) LOG("loadModel: cannot load texture " << path << ": " << stbi_failure_reason());
        return px;
    };

    for (const auto& modelMat : loader.getMaterials())
    {
        int aw = 1, ah = 1, nw = 1, nh = 1, mw = 1, mh = 1, ow = 1, oh = 1;

        unsigned char* aPixels = loadPixels(fullPath(modelMat.albedoPath), aw, ah);
        const unsigned char* aPx = aPixels ? aPixels : white;
        if (!aPixels) { aw = 1; ah = 1; }

        unsigned char* nPixels = loadPixels(fullPath(modelMat.normalPath), nw, nh);
        const unsigned char* nPx = nPixels ? nPixels : normalPx;
        if (!nPixels) { nw = 1; nh = 1; }

        unsigned char* mPixels = loadPixels(fullPath(modelMat.metallicRoughnessPath), mw, mh);
        const unsigned char* mPx = mPixels ? mPixels : defaultMR;
        if (!mPixels) { mw = 1; mh = 1; }

        unsigned char* oPixels = loadPixels(fullPath(modelMat.aoPath), ow, oh);
        const unsigned char* oPx = oPixels ? oPixels : white;
        if (!oPixels) { ow = 1; oh = 1; }

        auto mat = std::make_shared<Material>();
        mat->loadFromPixels(m_renderer.getDevice(),
            aPx, aw, ah, nPx, nw, nh, mPx, mw, mh, oPx, ow, oh,
            black, 1, 1, false);

        createMaterialDescriptorSet(mat);
        m_resources.registerMaterial(mat);
        result.materials.push_back(mat);

        if (aPixels) stbi_image_free(aPixels);
        if (nPixels) stbi_image_free(nPixels);
        if (mPixels) stbi_image_free(mPixels);
        if (oPixels) stbi_image_free(oPixels);
    }

    return result;
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

    decal->loadTexture(m_renderer.getDevice(),
        texturePath, opacityPath, normalPath, mrPath);

    LOG("using layout ptr=" << (void*)(VkDescriptorSetLayout)m_renderer.getDecalDescriptorSetLayout());

    decal->createDescriptorSet(m_renderer.getDevice(),
        m_renderer.getDecalDescriptorSetLayout(),
        m_renderer.getDepthView(), m_renderer.getDepthSampler(),
        m_renderer.getDecalCameraBuffer(), m_renderer.getLightBuffer(), m_renderer.getFramesInFlight());

    m_renderer.addScreenSpaceDecal(decal);
    return decal;
}

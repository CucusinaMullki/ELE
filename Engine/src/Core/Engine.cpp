#include "Core/Engine.h"
#include "Core/Log.h"

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

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* win, int, int) {
        static_cast<Engine*>(glfwGetWindowUserPointer(win))->onFramebufferResize();
    });

    m_renderer.initialize(m_window);

    m_resources.initialize(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
        m_renderer.getCommandPool(), m_renderer.getGraphicsQueue());

    m_input.initialize(m_window);
    m_camera.initialize(glm::vec3(0.0f, 0.0f, 3.0f));
    m_camera.setAspect(m_renderer.getAspect());
}

void Engine::onFramebufferResize()
{
    m_renderer.onResize();
    m_camera.setAspect(m_renderer.getAspect());
}

std::shared_ptr<Mesh> Engine::loadMesh(const std::string& path)
{
    return m_resources.loadMesh(path);
}

std::shared_ptr<Material> Engine::loadMaterial(const std::string& albedo, const std::string& normal,
    const std::string& metallic, const std::string& roughness)
{
    auto material = m_resources.loadMaterial(albedo, normal, metallic, roughness);

    material->createDescriptorSet(m_renderer.getDevice(),
        m_renderer.getDescriptorPool(), m_renderer.getDescriptorSetLayout(),
        m_renderer.getUboInfos(), m_renderer.getLightInfos(),
        m_renderer.getShadowView(), m_renderer.getShadowSampler(),
        m_renderer.getPointShadowView(), m_renderer.getPointShadowSampler(),
        m_renderer.getFramesInFlight());

    return material;
}

void Engine::updateInput(float deltaTime)
{
    m_input.update();

    glm::vec3 moveDir(0.0f);
    if (m_input.isKeyPressed(Key::W)) moveDir += m_camera.getFront();
    if (m_input.isKeyPressed(Key::S)) moveDir -= m_camera.getFront();
    if (m_input.isKeyPressed(Key::A)) moveDir -= m_camera.getRight();
    if (m_input.isKeyPressed(Key::D)) moveDir += m_camera.getRight();
    if (m_input.isKeyPressed(Key::E)) moveDir += m_camera.getUp();
    if (m_input.isKeyPressed(Key::Q)) moveDir -= m_camera.getUp();

    if (glm::length(moveDir) > 0.0f)
        m_camera.processKeyboard(glm::normalize(moveDir), deltaTime);

    m_camera.processMouse(m_input.getMouseDeltaX(), m_input.getMouseDeltaY());
}

void Engine::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();

        float now = static_cast<float>(glfwGetTime());
        float deltaTime = now - m_lastFrameTime;
        m_lastFrameTime = now;

        updateInput(deltaTime);
        m_renderer.setLights(m_lights);
        m_renderer.drawScene(m_scene, m_camera);
    }

    m_renderer.getDevice().waitIdle();
}

void Engine::shutdown()
{
    if (m_cleaned) return;
    m_cleaned = true;

    m_renderer.getDevice().waitIdle();

    m_resources.cleanup();
    m_renderer.cleanup();

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

        mat->createDescriptorSet(m_renderer.getDevice(),
            m_renderer.getDescriptorPool(), m_renderer.getDescriptorSetLayout(),
            m_renderer.getUboInfos(), m_renderer.getLightInfos(),
            m_renderer.getShadowView(), m_renderer.getShadowSampler(),
            m_renderer.getPointShadowView(), m_renderer.getPointShadowSampler(),
            m_renderer.getFramesInFlight());

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

    if (materials.empty())
    {
        auto mat = std::make_shared<Material>();
        mat->loadFromPixels(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
            m_renderer.getCommandPool(), m_renderer.getGraphicsQueue(),
            white, 1, 1, normalPx, 1, 1, white, 1, 1, white, 1, 1, black, 1, 1, false);
        mat->createDescriptorSet(m_renderer.getDevice(),
            m_renderer.getDescriptorPool(), m_renderer.getDescriptorSetLayout(),
            m_renderer.getUboInfos(), m_renderer.getLightInfos(),
            m_renderer.getShadowView(), m_renderer.getShadowSampler(),
            m_renderer.getPointShadowView(), m_renderer.getPointShadowSampler(),
            m_renderer.getFramesInFlight());
        m_resources.registerMaterial(mat);
        materials.push_back(mat);
    }

    auto& obj = m_scene.addObject();
    obj.mesh = mesh;
    obj.materials = materials;
    return obj;
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
    glm::vec3 position, glm::vec3 rotationEuler, glm::vec3 size)
{
    LOG("addScreenSpaceDecal called!");

    auto decal = std::make_shared<ScreenSpaceDecal>();
    decal->position = position;
    decal->rotationEuler = rotationEuler;
    decal->size = size;

    decal->loadTexture(m_renderer.getDevice(), m_renderer.getPhysicalDevice(),
        m_renderer.getCommandPool(), m_renderer.getGraphicsQueue(), texturePath);

    LOG("using layout ptr=" << (void*)(VkDescriptorSetLayout)m_renderer.getDecalDescriptorSetLayout());

    decal->createDescriptorSet(m_renderer.getDevice(), m_renderer.getDecalDescriptorPool(),
        m_renderer.getDecalDescriptorSetLayout(), m_renderer.getDepthView(), m_renderer.getDepthSampler(),
        m_renderer.getDecalCameraBuffer(), m_renderer.getFramesInFlight());

    m_renderer.addScreenSpaceDecal(decal);
    return decal;
}
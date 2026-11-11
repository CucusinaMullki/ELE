#pragma once
#include "Scene/ComponentRegistry.h"
#include "Core/BinaryStream.h"
#include <string>

class Scene;
class GameObject;

class ISceneAssets
{
public:
    virtual ~ISceneAssets() = default;

    virtual void applyAsset(GameObject& obj) = 0;

    virtual void finalize(GameObject&) {}

    virtual bool loadLevel(const std::string& levelPath) = 0;
};

namespace SceneSerializer
{
    constexpr uint32_t kVersion = 2;

    bool save(Scene& scene, const std::string& path, const std::string& levelPath);

    bool load(Scene& scene, const std::string& path, EngineContext& ctx, ISceneAssets& assets);

    void writeObject(BinaryWriter& w, GameObject& obj);
    GameObject* readObject(BinaryReader& r, Scene& scene, EngineContext& ctx, ISceneAssets& assets);

    void writeEntities(BinaryWriter& w, Scene& scene);
    uint32_t readEntities(BinaryReader& r, Scene& scene, EngineContext& ctx, ISceneAssets& assets);

}

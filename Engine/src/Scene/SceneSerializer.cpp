#include "Scene/SceneSerializer.h"
#include "Scene/PropertyIO.h"
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Core/BinaryStream.h"
#include "Core/Log.h"
#include <cstring>
#include <vector>

namespace
{
    constexpr char kMagic[4] = { 'E', 'S', 'C', 'N' };

    enum ObjectFlags : uint8_t
    {
        FlagDynamic = 1 << 0,
    };

    bool skipObject(const GameObject& o) { return o.isDecal || o.fromLevel; }

    void writeObjectImpl(BinaryWriter& w, GameObject& o)
    {
        const size_t blk = w.beginBlock();

        w.writeString(o.name);
        w.writeString(o.entityType);
        w.write<uint8_t>(o.isDynamic ? FlagDynamic : 0);

        w.write(o.transform.position);
        w.write(o.transform.rotation);
        w.write(o.transform.scale);

        w.write<uint8_t>((uint8_t)o.asset.kind);
        w.writeString(o.asset.path);
        w.write<int32_t>(o.asset.index);

        const size_t countAt = w.size();
        w.write<uint32_t>(0);
        uint32_t count = 0;

        o.forEachComponent([&](IComponent& c)
        {
            const char* type = c.typeName();
            if (!type) return;

            w.write<uint32_t>(propHash(type));
            const size_t cblk = w.beginBlock();
            BinaryPropertyWriter pw(w);
            pw.begin();
            c.visit(pw);
            pw.end();
            w.endBlock(cblk);
            ++count;
        });

        w.patch<uint32_t>(countAt, count);
        w.endBlock(blk);
    }

    GameObject* readObjectImpl(BinaryReader r, Scene& scene, EngineContext& ctx, ISceneAssets& assets)
    {
        GameObject& o = scene.addObject();

        o.name       = r.readString();
        o.entityType = r.readString();
        const uint8_t flags = r.read<uint8_t>();
        o.isDynamic = (flags & FlagDynamic) != 0;

        o.transform.position = r.read<glm::vec3>();
        o.transform.rotation = r.read<glm::vec3>();
        o.transform.scale    = r.read<glm::vec3>();

        o.asset.kind  = (AssetRef::Kind)r.read<uint8_t>();
        o.asset.path  = r.readString();
        o.asset.index = r.read<int32_t>();
        if (!r.ok()) return nullptr;

        if (!o.asset.empty())
            assets.applyAsset(o);

        const uint32_t count = r.read<uint32_t>();
        for (uint32_t i = 0; i < count && r.ok(); ++i)
        {
            const uint32_t typeHash = r.read<uint32_t>();
            BinaryReader block = r.readBlock();
            if (!r.ok()) return nullptr;

            const ComponentRegistry::Entry* e = ComponentRegistry::instance().find(typeHash);
            if (!e)
            {
                LOG("SceneSerializer: '" << o.name << "': unknown component 0x"
                    << std::hex << typeHash << std::dec << ", skipped");
                continue;
            }

            IComponent* c = e->create(o, ctx);
            if (!c) continue;

            BinaryPropertyReader pr(block);
            if (!pr.ok())
                LOG("SceneSerializer: '" << o.name << "': damaged " << e->name << ", defaults used");
            c->visit(pr);
            c->onPropertiesChanged();
        }

        assets.finalize(o);
        return r.ok() ? &o : nullptr;
    }
}

bool SceneSerializer::save(Scene& scene, const std::string& path, const std::string& levelPath)
{
    scene.flush();

    BinaryWriter w;
    w.writeBytes(kMagic, 4);
    w.write<uint32_t>(kVersion);
    w.writeString(levelPath);

    const size_t countAt = w.size();
    writeEntities(w, scene);
    uint32_t count = 0;
    std::memcpy(&count, w.data().data() + countAt, sizeof(count));

    if (!w.saveToFile(path))
    {
        LOG("SceneSerializer: cannot write " << path);
        return false;
    }
    LOG("SceneSerializer: saved " << count << " entities -> " << path
        << (levelPath.empty() ? "" : " (level " + levelPath + ")"));
    return true;
}

bool SceneSerializer::load(Scene& scene, const std::string& path, EngineContext& ctx, ISceneAssets& assets)
{
    std::vector<uint8_t> bytes;
    if (!BinaryReader::loadFile(path, bytes))
    {
        LOG("SceneSerializer: cannot open " << path);
        return false;
    }

    BinaryReader r(bytes.data(), bytes.size());
    char magic[4] = {};
    for (char& ch : magic) ch = (char)r.read<uint8_t>();
    const uint32_t version = r.read<uint32_t>();
    if (!r.ok() || std::memcmp(magic, kMagic, 4) != 0)
    {
        LOG("SceneSerializer: not an .escene file: " << path);
        return false;
    }
    if (version > kVersion)
    {
        LOG("SceneSerializer: " << path << " is version " << version
            << ", engine supports up to " << kVersion);
        return false;
    }

    const std::string levelPath = version >= 2 ? r.readString() : std::string();
    if (!r.ok()) return false;
    if (!levelPath.empty() && !assets.loadLevel(levelPath))
        LOG("SceneSerializer: level '" << levelPath << "' failed to load, entities only");

    const uint32_t loaded = readEntities(r, scene, ctx, assets);

    scene.flush();
    LOG("SceneSerializer: loaded " << loaded << " entities <- " << path);
    return r.ok();
}

void SceneSerializer::writeEntities(BinaryWriter& w, Scene& scene)
{
    scene.flush();
    const size_t countAt = w.size();
    w.write<uint32_t>(0);
    uint32_t count = 0;
    for (const auto& obj : scene.getObjects())
    {
        if (skipObject(*obj)) continue;
        writeObjectImpl(w, *obj);
        ++count;
    }
    w.patch<uint32_t>(countAt, count);
}

uint32_t SceneSerializer::readEntities(BinaryReader& r, Scene& scene, EngineContext& ctx, ISceneAssets& assets)
{
    const uint32_t count = r.read<uint32_t>();
    uint32_t loaded = 0;
    for (uint32_t i = 0; i < count && r.ok(); ++i)
    {
        BinaryReader block = r.readBlock();
        if (!r.ok()) break;
        if (readObjectImpl(block, scene, ctx, assets)) ++loaded;
        else LOG("SceneSerializer: entity #" << i << " is damaged");
    }
    scene.flush();
    return loaded;
}


void SceneSerializer::writeObject(BinaryWriter& w, GameObject& obj)
{
    writeObjectImpl(w, obj);
}

GameObject* SceneSerializer::readObject(BinaryReader& r, Scene& scene, EngineContext& ctx, ISceneAssets& assets)
{
    BinaryReader block = r.readBlock();
    if (!r.ok()) return nullptr;
    return readObjectImpl(block, scene, ctx, assets);
}

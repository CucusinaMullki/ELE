#include "Core/LevelWriter.h"
#include "Core/BinaryStream.h"
#include "Core/Log.h"
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Scene/PropertyIO.h"
#include "Components/LightComponent.h"
#include "Components/SpawnPointComponent.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
    void writeTrs(BinaryWriter& w, const Transform& t)
    {
        w.write(t.position);
        w.write(t.rotation);
        w.write(t.scale);
    }

    void writeProps(BinaryWriter& w, const std::unordered_map<std::string, std::string>& props)
    {
        std::vector<const std::pair<const std::string, std::string>*> sorted;
        for (const auto& kv : props) sorted.push_back(&kv);
        std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->first < b->first; });

        w.write<uint32_t>((uint32_t)sorted.size());
        for (const auto* kv : sorted)
        {
            w.writeString(kv->first);
            w.writeString(kv->second);
        }
    }

    struct Section { const uint8_t* data; size_t size; };

    Section sectionAt(const std::vector<uint8_t>& file, uint64_t off, const std::vector<uint64_t>& all)
    {
        uint64_t end = file.size();
        for (uint64_t o : all)
            if (o > off && o < end) end = o;
        return { file.data() + off, (size_t)(end - off) };
    }
}

bool LevelWriter::save(const std::string& srcElvl, const std::string& outElvl, Scene& scene)
{
    std::vector<uint8_t> src;
    if (!BinaryReader::loadFile(srcElvl, src))
    {
        LOG("LevelWriter: cannot open " << srcElvl);
        return false;
    }

    BinaryReader r(src.data(), src.size());
    char magic[4];
    for (char& ch : magic) ch = (char)r.read<uint8_t>();
    const uint32_t version = r.read<uint32_t>();
    std::vector<uint64_t> offs(version >= 2 ? 5 : 4);
    for (uint64_t& o : offs) o = r.read<uint64_t>();
    if (!r.ok() || std::memcmp(magic, "ELVL", 4) != 0 ||
        std::any_of(offs.begin(), offs.end(), [&](uint64_t o) { return o >= src.size(); }))
    {
        LOG("LevelWriter: " << srcElvl << " is not a valid .elvl");
        return false;
    }

    const Section meshes = sectionAt(src, offs[0], offs);
    const Section materials = sectionAt(src, offs[1], offs);

    BinaryWriter objW, lightW, spawnW;
    uint32_t nObj = 0, nLight = 0, nSpawn = 0;
    objW.write<uint32_t>(0); lightW.write<uint32_t>(0); spawnW.write<uint32_t>(0);

    scene.flush();
    for (const auto& sp : scene.getObjects())
    {
        GameObject& o = *sp;
        if (!o.fromLevel || o.isDecal) continue;

        if (auto* spawn = o.getComponent<SpawnPointComponent>())
        {
            spawnW.writeString(o.name);
            spawnW.writeString(spawn->spawnType);
            writeTrs(spawnW, o.transform);
            writeProps(spawnW, o.levelProperties);
            ++nSpawn;
        }
        else if (auto* light = o.getComponent<LightComponent>(); light && !o.mesh)
        {
            lightW.writeString(o.name);
            writeTrs(lightW, o.transform);
            const size_t blk = lightW.beginBlock();
            BinaryPropertyWriter pw(lightW);
            pw.begin();
            light->visit(pw);
            pw.end();
            lightW.endBlock(blk);
            ++nLight;
        }
        else
        {
            objW.writeString(o.name);
            objW.write<int32_t>(o.asset.kind == AssetRef::Kind::LevelMesh ? o.asset.index : -1);
            writeTrs(objW, o.transform);
            writeProps(objW, o.levelProperties);
            ++nObj;
        }
    }
    objW.patch<uint32_t>(0, nObj);
    lightW.patch<uint32_t>(0, nLight);
    spawnW.patch<uint32_t>(0, nSpawn);

    constexpr uint64_t kHeader = 4 + 4 + 8 * 5;
    const uint64_t oMeshes = kHeader;
    const uint64_t oMaterials = oMeshes + meshes.size;
    const uint64_t oObjects = oMaterials + materials.size;
    const uint64_t oLights = oObjects + objW.size();
    const uint64_t oSpawns = oLights + lightW.size();

    BinaryWriter w;
    w.writeBytes("ELVL", 4);
    w.write<uint32_t>(kVersion);
    for (uint64_t o : { oMeshes, oMaterials, oObjects, oLights, oSpawns })
        w.write<uint64_t>(o);
    w.writeBytes(meshes.data, meshes.size);
    w.writeBytes(materials.data, materials.size);
    w.writeBytes(objW.data().data(), objW.size());
    w.writeBytes(lightW.data().data(), lightW.size());
    w.writeBytes(spawnW.data().data(), spawnW.size());

    if (!w.saveToFile(outElvl))
    {
        LOG("LevelWriter: cannot write " << outElvl);
        return false;
    }
    LOG("LevelWriter: " << outElvl << ": objects=" << nObj << " lights=" << nLight << " spawns=" << nSpawn);
    return true;
}

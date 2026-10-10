#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

enum ParticleFlags : uint32_t
{
    PFLAG_LOOP = 1,
    PFLAG_ADDITIVE = 2,
    PFLAG_SOFT = 4,
    PFLAG_PROBES = 8,
    PFLAG_RADIAL = 16,
    PFLAG_PREWARM = 32,
};

#pragma pack(push, 1)
struct ParticleEffectRecord
{
    char name[32];
    char texture[96];
    float position[3];
    float basis[9];
    uint32_t seed;
    uint32_t particleCount;
    float rate;
    uint32_t flags;
    uint32_t shape;
    float shapeSize[3];
    float coneAngle;
    float life[2];
    float speed[2];
    float gravity[3];
    float drag;
    float sizeStart[2];
    float sizeEnd[2];
    float rotStart[2];
    float rotSpeed[2];
    float wobbleAmp;
    float wobbleFreq;
    float stops[4];
    float colors[16];
    uint32_t flipCols;
    uint32_t flipRows;
    float flipFps;
    float softDistance;
    float probeInfluence;
    float duration;
};
#pragma pack(pop)
static_assert(sizeof(ParticleEffectRecord) == 388, "ParticleEffectRecord layout");

class ParticlesLoader
{
public:
    static bool load(const std::string& path, std::vector<ParticleEffectRecord>& out, std::string* error = nullptr)
    {
        auto fail = [&](const char* m) { if (error) *error = m; return false; };
        out.clear();

        std::ifstream f(path, std::ios::binary);
        if (!f) return fail("cannot open file");

        struct Header { char magic[4]; uint32_t version, count, recordSize; } h{};
        f.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!f || std::memcmp(h.magic, "EPAR", 4) != 0) return fail("bad magic");
        if (h.version != 1) return fail("unsupported version");
        if (h.recordSize < sizeof(ParticleEffectRecord)) return fail("record too small");
        if (h.count > 4096) return fail("too many effects");

        std::vector<char> buf(h.recordSize);
        for (uint32_t i = 0; i < h.count; ++i)
        {
            f.read(buf.data(), h.recordSize);
            if (!f) return fail("truncated file");
            ParticleEffectRecord r;
            std::memcpy(&r, buf.data(), sizeof(r));
            r.name[31] = '\0';
            r.texture[95] = '\0';
            out.push_back(r);
        }
        return true;
    }
};
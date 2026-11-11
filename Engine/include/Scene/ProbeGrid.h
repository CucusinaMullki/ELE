#pragma once
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct ProbeGrid
{
    glm::vec3 origin{0.0f};
    float spacing = 1.0f;
    glm::uvec3 dims{0};
    std::vector<float> data;

    static constexpr uint32_t kFloatsPerProbe = 28;


    uint32_t probeCount() const { return dims.x * dims.y * dims.z; }
    bool empty() const { return data.empty(); }
    size_t byteSize() const { return data.size() * sizeof(float); }

    bool load(const std::string& path)
    {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) { std::fprintf(stderr, "[Probes] cannot open %s\n", path.c_str()); return false; }

        char magic[4];
        uint32_t version = 0;
        float o[3], sp;
        uint32_t d[3];
        bool ok = std::fread(magic, 1, 4, f) == 4
           && std::fread(&version, 4, 1, f) == 1
           && std::fread(o, 4, 3, f) == 3
           && std::fread(&sp, 4, 1, f) == 1
           && std::fread(d, 4, 3, f) == 3
           && std::string(magic, 4) == "EPRB" && version == 1;
        if (!ok) { std::fclose(f); std::fprintf(stderr, "[Probes] bad header in %s\n", path.c_str()); return false; }

        origin = {o[0], o[1], o[2]};
        spacing = sp;
        dims = {d[0], d[1], d[2]};

        data.resize(size_t(probeCount()) * kFloatsPerProbe);
        size_t got = std::fread(data.data(), sizeof(float), data.size(), f);
        std::fclose(f);
        if (got != data.size()) { data.clear(); std::fprintf(stderr, "[Probes] truncated file %s\n", path.c_str()); return false; }

        size_t valid = 0;
        for (uint32_t i = 0; i < probeCount(); ++i)
            valid += data[size_t(i) * kFloatsPerProbe + 27] > 0.5f;
        std::fprintf(stderr, "[Probes] %s: %ux%ux%u, spacing %.2f, valid %zu/%u, %zu KB\n",
            path.c_str(), dims.x, dims.y, dims.z, spacing, valid, probeCount(), byteSize() / 1024);
        return true;
    }
};

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <ktx.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

enum class Kind { Albedo, Normal, MR };

static bool classify(const std::string& stem, Kind& kind)
{
    std::string n = stem;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    auto has = [&](const char* s) { return n.find(s) != std::string::npos; };

    if (has("occlusionroughnessmetallic") || has("roughness") || has("metallic")) { kind = Kind::MR; return true; }
    if (has("normal")) { kind = Kind::Normal; return true; }
    if (has("basecolor") || has("albedo") || has("diffuse") || has("color")) { kind = Kind::Albedo; return true; }
    return false;
}

struct Image { int w = 0, h = 0; std::vector<uint8_t> px; };

static float g_srgbToLin[256];

static uint8_t linToSrgb(float l)
{
    l = std::clamp(l, 0.0f, 1.0f);
    float s = (l <= 0.0031308f) ? 12.92f * l : 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
    return (uint8_t)(std::clamp(s, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static Image downsample(const Image& src, Kind kind)
{
    Image dst;
    dst.w = std::max(1, src.w / 2);
    dst.h = std::max(1, src.h / 2);
    dst.px.resize((size_t)dst.w * dst.h * 4);

    for (int y = 0; y < dst.h; y++)
        for (int x = 0; x < dst.w; x++)
        {
            float acc[4] = { 0, 0, 0, 0 };
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                {
                    int sx = std::min(src.w - 1, x * 2 + dx);
                    int sy = std::min(src.h - 1, y * 2 + dy);
                    const uint8_t* p = &src.px[((size_t)sy * src.w + sx) * 4];
                    for (int c = 0; c < 3; c++)
                        acc[c] += (kind == Kind::Albedo) ? g_srgbToLin[p[c]] : p[c] / 255.0f;
                    acc[3] += p[3] / 255.0f;
                }
            for (float& a : acc) a *= 0.25f;

            uint8_t* o = &dst.px[((size_t)y * dst.w + x) * 4];
            if (kind == Kind::Albedo)
            {
                for (int c = 0; c < 3; c++) o[c] = linToSrgb(acc[c]);
            }
            else if (kind == Kind::Normal)
            {
                float v[3] = { acc[0] * 2 - 1, acc[1] * 2 - 1, acc[2] * 2 - 1 };
                float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (len > 1e-6f) for (float& c : v) c /= len;
                for (int c = 0; c < 3; c++)
                    o[c] = (uint8_t)(std::clamp((v[c] * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
            }
            else
            {
                for (int c = 0; c < 3; c++)
                    o[c] = (uint8_t)(std::clamp(acc[c] * 255.0f + 0.5f, 0.0f, 255.0f));
            }
            o[3] = (uint8_t)(std::clamp(acc[3] * 255.0f + 0.5f, 0.0f, 255.0f));
        }
    return dst;
}

static bool convertFile(const fs::path& src, Kind kind, uint64_t& srcBytes, uint64_t& outBytes)
{
    fs::path out = src;
    out.replace_extension(".ktx2");
    if (fs::exists(out) && fs::last_write_time(out) >= fs::last_write_time(src))
        return false;

    int w = 0, h = 0;
    stbi_uc* data = stbi_load(src.string().c_str(), &w, &h, nullptr, 4);
    if (!data) { std::fprintf(stderr, "[texconv] cannot read %s\n", src.string().c_str()); return false; }

    std::vector<Image> levels(1);
    levels[0].w = w;
    levels[0].h = h;
    levels[0].px.assign(data, data + (size_t)w * h * 4);
    stbi_image_free(data);

    if (kind == Kind::MR)
        for (size_t i = 0; i < (size_t)w * h; i++)
            levels[0].px[i * 4 + 2] /= 2;

    while (levels.back().w > 1 || levels.back().h > 1)
        levels.push_back(downsample(levels.back(), kind));

    ktxTextureCreateInfo ci{};
    ci.vkFormat = (kind == Kind::Albedo) ? 43u : 37u;
    ci.baseWidth = (ktx_uint32_t)w;
    ci.baseHeight = (ktx_uint32_t)h;
    ci.baseDepth = 1;
    ci.numDimensions = 2;
    ci.numLevels = (ktx_uint32_t)levels.size();
    ci.numLayers = 1;
    ci.numFaces = 1;
    ci.isArray = KTX_FALSE;
    ci.generateMipmaps = KTX_FALSE;

    ktxTexture2* tex = nullptr;
    if (ktxTexture2_Create(&ci, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &tex) != KTX_SUCCESS)
    { std::fprintf(stderr, "[texconv] ktxTexture2_Create failed: %s\n", src.string().c_str()); return false; }

    for (size_t i = 0; i < levels.size(); i++)
        ktxTexture_SetImageFromMemory(ktxTexture(tex), (ktx_uint32_t)i, 0, 0,
            levels[i].px.data(), levels[i].px.size());

    ktxBasisParams bp{};
    bp.structSize = sizeof(bp);
    bp.uastc = KTX_TRUE;
    bp.uastcFlags  = KTX_PACK_UASTC_LEVEL_DEFAULT;
    bp.threadCount = std::max(1u, std::thread::hardware_concurrency());

    bool ok = ktxTexture2_CompressBasisEx(tex, &bp) == KTX_SUCCESS
       && ktxTexture2_DeflateZstd(tex, 19) == KTX_SUCCESS
       && ktxTexture_WriteToNamedFile(ktxTexture(tex), out.string().c_str()) == KTX_SUCCESS;
    ktxTexture_Destroy(ktxTexture(tex));

    if (!ok) { std::fprintf(stderr, "[texconv] encode/write failed: %s\n", src.string().c_str()); return false; }

    srcBytes += fs::file_size(src);
    outBytes += fs::file_size(out);
    std::printf("[texconv] %s: %llu KB -> %llu KB\n", src.filename().string().c_str(),
        (unsigned long long)(fs::file_size(src) / 1024), (unsigned long long)(fs::file_size(out) / 1024));
    return true;
}

int main(int argc, char** argv)
{
    for (int i = 0; i < 256; i++)
    {
        float c = i / 255.0f;
        g_srgbToLin[i] = (c <= 0.04045f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    std::vector<fs::path> files;
    auto isImage = [](const fs::path& p) {
        std::string e = p.extension().string();
        std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return std::tolower(c); });
        return e == ".png" || e == ".jpg" || e == ".jpeg";
    };

    for (int i = 1; i < argc; i++)
    {
        fs::path p(argv[i]);
        std::error_code ec;
        if (fs::is_directory(p, ec))
        {
            for (const auto& e : fs::recursive_directory_iterator(p, ec))
                if (e.is_regular_file() && isImage(e.path())) files.push_back(e.path());
        }
        else if (fs::is_regular_file(p, ec) && isImage(p))
            files.push_back(p);
        else
            std::fprintf(stderr, "[texconv] skip (not found): %s\n", argv[i]);
    }

    uint64_t srcBytes = 0, outBytes = 0;
    int converted = 0;
    for (const auto& f : files)
    {
        Kind kind;
        if (!classify(f.stem().string(), kind))
        {
            std::fprintf(stderr, "[texconv] skip (unknown texture type): %s\n", f.string().c_str());
            continue;
        }
        if (convertFile(f, kind, srcBytes, outBytes)) converted++;
    }

    if (converted)
        std::printf("[texconv] done: %d converted, %llu KB -> %llu KB\n", converted,
        (unsigned long long)(srcBytes / 1024), (unsigned long long)(outBytes / 1024));
    return 0;
}
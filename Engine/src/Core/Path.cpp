#include "Core/Path.h"

#ifdef _WIN32
    #include <windows.h>
#elif __linux__
    #include <unistd.h>
    #include <limits.h>
#endif

std::filesystem::path Paths::s_assetDir;

std::filesystem::path Paths::executableDir()
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#elif __linux__
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1)
    {
        buf[len] = '\0';
        return std::filesystem::path(buf).parent_path();
    }
    return std::filesystem::current_path();
#endif
}

std::filesystem::path Paths::assetDir()
{
    if (!s_assetDir.empty())
        return s_assetDir;
    return executableDir();
}

void Paths::setAssetDir(const std::filesystem::path& path)
{
    s_assetDir = path;
}

std::filesystem::path Paths::resolveAsset(const std::string& raw, const std::filesystem::path& levelDir)
{
    namespace fs = std::filesystem;
    if (raw.empty()) return {};

    std::string p = raw;
    for (char& c : p) if (c == '\\') c = '/';
    fs::path fp(p);
    std::error_code ec;
    auto found = [&](const fs::path& x) {
        return fs::exists(x, ec) || fs::exists(fs::path(x).replace_extension(".ktx2"), ec);
    };

    if (fp.is_absolute() && found(fp)) return fp;
    if (fp.is_relative()) {
        if (found(assetDir() / fp)) return assetDir() / fp;
        if (!levelDir.empty() && found(levelDir / fp)) return levelDir / fp;
    }

    const fs::path name = fp.filename();
    for (const fs::path& c : { assetDir() / "Levels" / "textures" / name,
        levelDir / "textures" / name,
        levelDir / name })
        if (!c.empty() && found(c)) return c;

    std::fprintf(stderr, "[Paths] not found: %s\n", raw.c_str());
    return fp;
}

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
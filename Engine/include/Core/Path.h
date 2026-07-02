#pragma once
#include <filesystem>
#include <string>

class Paths
{
public:
    static std::filesystem::path executableDir();

    static std::filesystem::path assetDir();

    static void setAssetDir(const std::filesystem::path& path);

private:
    static std::filesystem::path s_assetDir;
};
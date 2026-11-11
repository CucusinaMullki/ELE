#pragma once
#include <cstdint>
#include <string>

class Scene;

namespace LevelWriter
{
    constexpr uint32_t kVersion = 2;

    bool save(const std::string& srcElvl, const std::string& outElvl, Scene& scene);
}

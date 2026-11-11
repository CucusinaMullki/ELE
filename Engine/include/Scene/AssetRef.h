#pragma once
#include <cstdint>
#include <string>

struct AssetRef
{
    enum class Kind : uint8_t
    {
        None = 0,
        LevelMesh = 1,
        Model = 2,
        Skeletal = 3,
    };

    Kind kind = Kind::None;
    std::string path;
    int32_t index = -1;

    bool empty() const { return kind == Kind::None; }
};

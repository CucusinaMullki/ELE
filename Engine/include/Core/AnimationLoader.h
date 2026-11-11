#pragma once
#include "Scene/Animation.h"
#include <string>

class AnimationLoader
{
public:
    static Animation load(const std::string& path);
};

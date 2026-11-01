#pragma once
#include "RT/RTPass.h"

namespace RT
{

class ReflectionPass : public Pass
{
protected:
    const char* raygenName() const override { return "reflection.rgen"; }
    std::vector<Output> outputs() const override
    {
        return { { 1, ASH::Format::R16G16B16A16_Float, "RT reflection", true } };
    }
};

}
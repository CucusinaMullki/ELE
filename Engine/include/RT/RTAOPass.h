#pragma once
#include "RT/RTPass.h"

namespace RT
{

class AOPass : public Pass
{
protected:
    const char* raygenName() const override { return "ao.rgen"; }
    std::vector<Output> outputs() const override
    {
        return { { 1, ASH::Format::R8G8B8A8_UNorm, "RT AO", true } };
    }
};

}
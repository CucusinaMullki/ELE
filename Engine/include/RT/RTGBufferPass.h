#pragma once
#include "RT/RTPass.h"

namespace RT
{

class GBufferPass : public Pass
{
protected:
    const char* raygenName() const override { return "gbuffer.rgen"; }
    std::vector<Output> outputs() const override
    {
        return { { 1, ASH::Format::R32G32B32A32_Float, "RT gbuffer", false } };
    }
};

}
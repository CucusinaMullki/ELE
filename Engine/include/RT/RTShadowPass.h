#pragma once
#include "RT/RTPass.h"

namespace RT
{

class ShadowPass : public Pass
{
public:
    ASH::Texture* mask(uint32_t frame) const { return image(frame); }

protected:
    const char* raygenName() const override { return "shadow_mask.rgen"; }
    std::vector<Output> outputs() const override
    {
        return { { 1, ASH::Format::R8G8B8A8_UNorm, "RT shadow mask", true } };
    }
};

}
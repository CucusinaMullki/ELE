#include "RT/RTNrd.h"

#ifdef ELE_HAS_NRD
#include <NRD.h>
#include <iostream>

namespace RT
{
namespace
{
const char* kFormatNames[] = {
    "R8_UNORM", "R8_SNORM", "R8_UINT", "R8_SINT",
    "RG8_UNORM", "RG8_SNORM", "RG8_UINT", "RG8_SINT",
    "RGBA8_UNORM", "RGBA8_SNORM", "RGBA8_UINT", "RGBA8_SINT", "RGBA8_SRGB",
    "R16_UNORM", "R16_SNORM", "R16_UINT", "R16_SINT", "R16_SFLOAT",
    "RG16_UNORM", "RG16_SNORM", "RG16_UINT", "RG16_SINT", "RG16_SFLOAT",
    "RGBA16_UNORM", "RGBA16_SNORM", "RGBA16_UINT", "RGBA16_SINT", "RGBA16_SFLOAT",
    "R32_UINT", "R32_SINT", "R32_SFLOAT",
    "RG32_UINT", "RG32_SINT", "RG32_SFLOAT",
    "RGB32_UINT", "RGB32_SINT", "RGB32_SFLOAT",
    "RGBA32_UINT", "RGBA32_SINT", "RGBA32_SFLOAT",
    "R10_G10_B10_A2_UNORM", "R10_G10_B10_A2_UINT", "R11_G11_B10_UFLOAT", "R9_G9_B9_E5_UFLOAT"
};

const char* fmtName(nrd::Format f)
{
    const uint32_t i = static_cast<uint32_t>(f);
    return i < sizeof(kFormatNames) / sizeof(kFormatNames[0]) ? kFormatNames[i] : "?";
}
}

void logNrdInstanceInfo()
{
    const nrd::LibraryDesc* lib = nrd::GetLibraryDesc();
    std::cout << "NRD: version " << int(lib->versionMajor) << "." << int(lib->versionMinor) << "."
              << int(lib->versionBuild)
              << ", normalEncoding=" << int(lib->normalEncoding)
              << ", roughnessEncoding=" << int(lib->roughnessEncoding)
              << ", spirv offsets (sampler/texture/cbuffer/storage) = "
              << lib->spirvBindingOffsets.samplerOffset << "/"
              << lib->spirvBindingOffsets.textureOffset << "/"
              << lib->spirvBindingOffsets.constantBufferOffset << "/"
              << lib->spirvBindingOffsets.storageTextureAndBufferOffset << "\n";

    nrd::DenoiserDesc dd{};
    dd.identifier = 0;
    dd.denoiser = nrd::Denoiser::REBLUR_DIFFUSE;

    nrd::InstanceCreationDesc cd{};
    cd.denoisers = &dd;
    cd.denoisersNum = 1;

    nrd::Instance* inst = nullptr;
    if (nrd::CreateInstance(cd, inst) != nrd::Result::SUCCESS || !inst)
    {
        std::cout << "NRD: CreateInstance FAILED\n";
        return;
    }

    const nrd::InstanceDesc* d = nrd::GetInstanceDesc(*inst);
    std::cout << "NRD: spaces cb+samplers=" << d->constantBufferAndSamplersSpaceIndex
              << " resources=" << d->resourcesSpaceIndex
              << ", cbReg=" << d->constantBufferRegisterIndex
              << ", samplersBase=" << d->samplersBaseRegisterIndex
              << ", resourcesBase=" << d->resourcesBaseRegisterIndex
              << ", cbMaxSize=" << d->constantBufferMaxDataSize
              << ", samplers=" << d->samplersNum
              << ", entry=" << d->shaderEntryPoint << "\n";

    for (uint32_t i = 0; i < d->samplersNum; ++i)
        std::cout << "NRD: sampler[" << i << "] = "
                  << (d->samplers[i] == nrd::Sampler::NEAREST_CLAMP ? "NEAREST_CLAMP" : "LINEAR_CLAMP") << "\n";

    std::cout << "NRD: pipelines = " << d->pipelinesNum << "\n";
    for (uint32_t i = 0; i < d->pipelinesNum; ++i)
    {
        const nrd::PipelineDesc& p = d->pipelines[i];
        std::cout << "NRD:  pipeline[" << i << "] " << p.shaderIdentifier
                  << " spirv=" << p.computeShaderSPIRV.size << "B"
                  << " cbuffer=" << (p.hasConstantData ? "yes" : "no") << " ranges:";
        for (uint32_t r = 0; r < p.resourceRangesNum; ++r)
            std::cout << " [" << (p.resourceRanges[r].descriptorType == nrd::DescriptorType::TEXTURE ? "TEX" : "UAV")
                      << " x" << p.resourceRanges[r].descriptorsNum << "]";
        std::cout << "\n";
    }

    std::cout << "NRD: permanent pool = " << d->permanentPoolSize << ", transient pool = " << d->transientPoolSize << "\n";
    for (uint32_t i = 0; i < d->permanentPoolSize; ++i)
        std::cout << "NRD:  permanent[" << i << "] " << fmtName(d->permanentPool[i].format)
                  << " downsample=" << d->permanentPool[i].downsampleFactor << "\n";
    for (uint32_t i = 0; i < d->transientPoolSize; ++i)
        std::cout << "NRD:  transient[" << i << "] " << fmtName(d->transientPool[i].format)
                  << " downsample=" << d->transientPool[i].downsampleFactor << "\n";

    std::cout << "NRD: descriptor pool desc: perSetTex=" << d->descriptorPoolDesc.perSetTexturesMaxNum
              << " perSetUav=" << d->descriptorPoolDesc.perSetStorageTexturesMaxNum
              << " totalTex=" << d->descriptorPoolDesc.totalTexturesNum
              << " totalUav=" << d->descriptorPoolDesc.totalStorageTexturesNum
              << " sets=" << d->descriptorPoolDesc.setsMaxNum << "\n";

    nrd::DestroyInstance(*inst);
}
}

#else

namespace RT { void logNrdInstanceInfo() {} }

#endif
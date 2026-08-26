#pragma once
#include "Scene/Skeleton.h"
#include "Scene/Animation.h"
#include "Vulkan/SkinnedVertex.h"
#include <tiny_gltf.h>
#include <string>
#include <vector>
#include <memory>

struct SkeletalGltfResult
{
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    std::shared_ptr<Skeleton> skeleton;
    std::vector<std::shared_ptr<Animation>> animations;
};

class SkeletalGltfLoader
{
public:
    static SkeletalGltfResult load(const std::string& path);

private:
    static std::shared_ptr<Skeleton> parseSkeleton(const tinygltf::Model& model, int skinIndex);
    static void parseVertexData(const tinygltf::Model& model, SkeletalGltfResult& result, const Skeleton& skeleton);
    static std::vector<std::shared_ptr<Animation>> parseAnimations(const tinygltf::Model& model, const Skeleton& skeleton);
};
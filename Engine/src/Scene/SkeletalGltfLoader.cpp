#include "Scene/SkeletalGltfLoader.h"
#include "Core/Path.h"
#include "Core/Log.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <unordered_map>

std::shared_ptr<Skeleton> SkeletalGltfLoader::parseSkeleton(const tinygltf::Model& model, int skinIndex)
{
    auto skeleton = std::make_shared<Skeleton>();

    const auto& skin = model.skins[skinIndex];

    std::vector<glm::mat4> inverseBindMatrices(skin.joints.size(), glm::mat4(1.0f));
    if (skin.inverseBindMatrices >= 0)
    {
        const auto& accessor = model.accessors[skin.inverseBindMatrices];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];
        const float* data = reinterpret_cast<const float*>(
            buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);

        for (size_t i = 0; i < skin.joints.size(); i++)
            inverseBindMatrices[i] = glm::make_mat4(data + i * 16);
    }

    std::unordered_map<int, int> nodeToJointIndex;
    for (size_t i = 0; i < skin.joints.size(); i++)
        nodeToJointIndex[skin.joints[i]] = (int)i;

    for (size_t i = 0; i < skin.joints.size(); i++)
    {
        int nodeIdx = skin.joints[i];
        const auto& node = model.nodes[nodeIdx];

        Joint joint{};
        joint.name = node.name.empty() ? ("joint_" + std::to_string(i)) : node.name;
        joint.inverseBindMatrix = inverseBindMatrices[i];

        glm::mat4 T = node.translation.size() == 3
            ? glm::translate(glm::mat4(1.0f), glm::vec3(node.translation[0], node.translation[1], node.translation[2]))
            : glm::mat4(1.0f);
        glm::mat4 R = node.rotation.size() == 4
            ? glm::toMat4(glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]))
            : glm::mat4(1.0f);
        glm::mat4 S = node.scale.size() == 3
            ? glm::scale(glm::mat4(1.0f), glm::vec3(node.scale[0], node.scale[1], node.scale[2]))
            : glm::mat4(1.0f);
        joint.localBindTransform = T * R * S;

        joint.parentIndex = -1;
        skeleton->addJoint(joint);
    }

    for (size_t i = 0; i < skin.joints.size(); i++)
    {
        int nodeIdx = skin.joints[i];
        for (const auto& [otherNodeIdx, otherJointIdx] : nodeToJointIndex)
        {
            const auto& otherNode = model.nodes[otherNodeIdx];
            for (int child : otherNode.children)
            {
                if (child == nodeIdx)
                    skeleton->setJointParent((int)i, otherJointIdx);
            }
        }
    }

    return skeleton;
}

void SkeletalGltfLoader::parseVertexData(const tinygltf::Model& model, SkeletalGltfResult& result, const Skeleton& skeleton)
{
    auto getAccessorPtr = [&model](int accessorIndex, size_t& count, size_t& stride) -> const unsigned char* {
        const auto& accessor = model.accessors[accessorIndex];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        count = accessor.count;
        int numComponents = tinygltf::GetNumComponentsInType(accessor.type);
        int componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
        stride = bufferView.byteStride > 0 ? bufferView.byteStride : (size_t)(numComponents * componentSize);

        return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    };

    for (const auto& mesh : model.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.indices < 0) continue;

            auto posIt = primitive.attributes.find("POSITION");
            auto normIt = primitive.attributes.find("NORMAL");
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            auto tanIt = primitive.attributes.find("TANGENT");
            auto jointsIt = primitive.attributes.find("JOINTS_0");
            auto weightsIt = primitive.attributes.find("WEIGHTS_0");

            if (posIt == primitive.attributes.end()) continue;
            if (jointsIt == primitive.attributes.end() || weightsIt == primitive.attributes.end())
            {
                LOG("SkeletalGltfLoader: primitive has no JOINTS_0/WEIGHTS_0, skipping");
                continue;
            }

            size_t vertexCount = 0, posStride = 0;
            const unsigned char* posData = getAccessorPtr(posIt->second, vertexCount, posStride);

            size_t dummy = 0, normStride = 0, uvStride = 0, tanStride = 0, jointStride = 0, weightStride = 0;
            const unsigned char* normData = normIt != primitive.attributes.end() ? getAccessorPtr(normIt->second, dummy, normStride) : nullptr;
            const unsigned char* uvData = uvIt != primitive.attributes.end() ? getAccessorPtr(uvIt->second, dummy, uvStride) : nullptr;
            const unsigned char* tanData = tanIt != primitive.attributes.end() ? getAccessorPtr(tanIt->second, dummy, tanStride) : nullptr;

            LOG("SkeletalGltfLoader: primitive has NORMAL=" << (normIt != primitive.attributes.end())
                << " TANGENT=" << (tanIt != primitive.attributes.end()));

            const unsigned char* jointData = getAccessorPtr(jointsIt->second, dummy, jointStride);
            const unsigned char* weightsData = getAccessorPtr(weightsIt->second, dummy, weightStride);

            const auto& jointsAccessor = model.accessors[jointsIt->second];
            const auto& weightsAccessor = model.accessors[weightsIt->second];

            uint32_t vertexStart = (uint32_t)result.vertices.size();

            for (size_t i = 0; i < vertexCount; i++)
            {
                SkinnedVertex v{};
                std::memcpy(&v.pos, posData + i * posStride, sizeof(glm::vec3));

                if (normData)
                    std::memcpy(&v.normal, normData + i * normStride, sizeof(glm::vec3));
                else
                    v.normal = glm::vec3(0, 1, 0);

                if (uvData)
                    std::memcpy(&v.texCoord, uvData + i * uvStride, sizeof(glm::vec2));

                if (tanData)
                {
                    glm::vec4 t{};
                    std::memcpy(&t, tanData + i * tanStride, sizeof(glm::vec4));
                    v.tangent = glm::vec3(t);
                }
                else
                {
                    glm::vec3 up = (std::abs(v.normal.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                    v.tangent = glm::normalize(glm::cross(up, v.normal));
                }

                glm::ivec4 joints(0);
                const unsigned char* jp = jointData + i * jointStride;
                if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    const uint16_t* jp16 = reinterpret_cast<const uint16_t*>(jp);
                    joints = glm::ivec4(jp[0], jp[1], jp[2], jp[3]);
                }
                v.boneIndices = joints;

                glm::vec4 weights(0.0f);
                const unsigned char* wp = weightsData + i * weightStride;
                if (weightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                {
                    std::memcpy(&weights, wp, sizeof(glm::vec4));
                }
                else if (weightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                {
                    weights = glm::vec4(wp[0], wp[1], wp[2], wp[3]) / 255.0f;
                }
                else if (weightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const uint16_t* wp16 = reinterpret_cast<const uint16_t*>(wp);
                    weights = glm::vec4(wp16[0], wp16[1], wp16[2], wp16[3]) / 65535.0f;
                }
                v.boneWeights = weights;

                result.vertices.push_back(v);
            }

            const auto& idxAccessor = model.accessors[primitive.indices];
            size_t idxCount = 0, idxStride = 0;
            const unsigned char* idxData = getAccessorPtr(primitive.indices, idxCount, idxStride);

            for (size_t i = 0; i < idxCount; i++)
            {
                uint32_t index = 0;
                const unsigned char* p = idxData + i * idxStride;
                switch (idxAccessor.componentType)
                {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: index = *reinterpret_cast<const uint32_t*>(p); break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: index = *reinterpret_cast<const uint16_t*>(p); break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  index = *p; break;
                    default: throw std::runtime_error("unsupported glTF index type");
                }
                result.indices.push_back(vertexStart + index);
            }
        }
    }
}

std::vector<std::shared_ptr<Animation>> SkeletalGltfLoader::parseAnimations(const tinygltf::Model& model, const Skeleton& skeleton)
{
    std::vector<std::shared_ptr<Animation>> result;

    auto getAccessorFloatData = [&model](int accessorIndex) -> std::vector<float>
    {
        const auto& accessor = model.accessors[accessorIndex];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        const unsigned char* raw = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        int numComponents = tinygltf::GetNumComponentsInType(accessor.type);

        std::vector<float> out(accessor.count * numComponents);
        std::memcpy(out.data(), raw, out.size() * sizeof(float));
        return out;
    };

    for (const auto& gltfAnim : model.animations)
    {
        auto animation = std::make_shared<Animation>();
        animation->name = gltfAnim.name.empty() ? "unnamed_animation" : gltfAnim.name;

        std::unordered_map<std::string, JointAnimationChannel> channelsByJoint;

        for (const auto& channel : gltfAnim.channels)
        {
            int targetNode = channel.target_node;
            if (targetNode < 0) continue;

            std::string jointName = model.nodes[targetNode].name;
            if (jointName.empty()) continue;

            const auto& sampler = gltfAnim.samplers[channel.sampler];

            std::vector<float> times = getAccessorFloatData(sampler.input);
            std::vector<float> values = getAccessorFloatData(sampler.output);

            auto& jointChannel = channelsByJoint[jointName];
            jointChannel.jointName = jointName;

            if (channel.target_path == "translation")
            {
                for (size_t i = 0; i < times.size(); i++)
                {
                    glm::vec3 pos(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
                    jointChannel.poisitonKeys.push_back({ times[i], pos });
                }
            }
            else if (channel.target_path == "rotation")
            {
                for (size_t i = 0; i < times.size(); i++)
                {
                    glm::quat rot(values[i * 4 + 3], values[i * 4 + 0], values[i * 4 + 1], values[i * 4 + 2]);
                    jointChannel.rotationKeys.push_back({ times[i], rot });
                }
            }
            else if (channel.target_path == "scale")
            {
                for (size_t i = 0; i < times.size(); i++)
                {
                    glm::vec3 scale(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
                    jointChannel.scaleKeys.push_back({ times[i], scale });
                }
            }
            
            animation->duration = std::max(animation->duration, times.empty() ? 0.0f : times.back());
        }

        for (auto& [name, channel] : channelsByJoint)
            animation->channels.push_back(std::move(channel));

        result.push_back(animation);
        LOG("SkeletalGltfLoader: parsed animation '" << animation->name << "', duration=" << animation->duration
            << ", channels=" << animation->channels.size());
    }

    return result;
}

SkeletalGltfResult SkeletalGltfLoader::load(const std::string& path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    std::string fullPath = (Paths::assetDir() / path).string();
    bool isBinary = fullPath.size() > 4 && fullPath.substr(fullPath.size() - 4) == ".glb";

    bool ok = isBinary
        ? loader.LoadBinaryFromFile(&model, &err, &warn, fullPath)
        : loader.LoadASCIIFromFile(&model, &err, &warn, fullPath);

    if (!warn.empty()) LOG("SkeletalGltfLoader warning: " << warn);
    if (!ok) throw std::runtime_error("failed to load skeletal glTF: " + fullPath + " " + err);

    if (model.skins.empty())
        throw std::runtime_error("glTF file has no skins: " + fullPath);

    SkeletalGltfResult result;
    result.skeleton = parseSkeleton(model, 0);
    parseVertexData(model, result, *result.skeleton);
    result.animations = parseAnimations(model, *result.skeleton);

    LOG("SkeletalGltfLoader: loaded " << result.vertices.size() << " verts, "
        << result.indices.size() << " indices, " << result.skeleton->getJointCount() << " joints, "
        << result.animations.size() << " animations");

    return result;
}
#version 450

layout(binding = 9) uniform BoneMatrices {
    mat4 bones[100];
} boneData;

layout(push_constant) uniform Push {
    mat4 lightSpaceMatrix;
    mat4 model;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inBoneIndices;
layout(location = 5) in vec4 inBoneWeights;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    ivec4 boneIdx = ivec4(inBoneIndices);
    mat4 skinMatrix =
        inBoneWeights.x * boneData.bones[boneIdx.x] +
        inBoneWeights.y * boneData.bones[boneIdx.y] +
        inBoneWeights.z * boneData.bones[boneIdx.z] +
        inBoneWeights.w * boneData.bones[boneIdx.w];

    vec4 skinnedLocalPos = skinMatrix * vec4(inPosition, 1.0);
    gl_Position = push.lightSpaceMatrix * push.model * skinnedLocalPos;
}

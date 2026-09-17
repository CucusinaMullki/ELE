#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inBoneIndices;
layout(location = 5) in vec4 inBoneWeights;

layout(binding = 6) uniform BoneMatrices
{
    mat4 bones[100];
} boneData;

layout(push_constant) uniform Push 
{
    mat4 model;
    vec4 voxelGridMin;
    vec4 voxelGridSize;
} push;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main()
{
    ivec4 boneIdx = ivec4(inBoneIndices);

    mat4 skinMatrix =
        inBoneWeights.x * boneData.bones[boneIdx.x] +
        inBoneWeights.y * boneData.bones[boneIdx.y] +
        inBoneWeights.z * boneData.bones[boneIdx.z] +
        inBoneWeights.w * boneData.bones[boneIdx.w];

    vec4 skinnedLocalPos = skinMatrix * vec4(inPosition, 1.0);
    vec4 worldPos = push.model * skinnedLocalPos;

    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(push.model) * mat3(skinMatrix) * inNormal;
    fragTexCoord = inTexCoord;

    vec3 normalizedPos = (worldPos.xyz - push.voxelGridMin.xyz) / push.voxelGridSize.xyz;
    gl_Position = vec4(normalizedPos.xy * 2.0 - 1.0, normalizedPos.z, 1.0);
}
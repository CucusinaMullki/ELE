#version 450
#extension GL_GOOGLE_include_directive : require

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    mat4 prevViewProj;
    mat4 viewProjNoJitter;
    vec4 camPos;
} cam;

layout(binding = 9) uniform BoneMatrices {
    mat4 bones[100];
} boneData;

layout(push_constant) uniform Push
{
    mat4 model;
    mat4 normalMatrix;
    mat4 prevModel;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inBoneIndices;
layout(location = 5) in vec4 inBoneWeights;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragLightSpacePos;
layout(location = 3) out mat3 fragTBN;

layout(location = 6) out vec4 fragCurrClipPos;
layout(location = 7) out vec4 fragPrevClipPos;
layout(location = 8) out vec2 fragLightmapUV;

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
    gl_Position = cam.proj * cam.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragTexCoord = inTexCoord;
    fragLightSpacePos = cam.lightSpaceMatrix * worldPos;
    fragCurrClipPos = cam.viewProjNoJitter * worldPos;

    vec4 prevWorldPos = push.prevModel * skinnedLocalPos;
    fragPrevClipPos = cam.prevViewProj * prevWorldPos;

    fragLightmapUV = vec2(0.0);

    mat3 skinNormalMat = mat3(skinMatrix);
    mat3 nm = mat3(push.normalMatrix);

    vec3 N = normalize(nm * (skinNormalMat * inNormal));
    vec3 T = normalize(nm * (skinNormalMat * inTangent));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    fragTBN = mat3(T, B, N);
}
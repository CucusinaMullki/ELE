#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D albedoMap;
layout(binding = 1, rgba8) uniform image3D voxelTexture;

layout(push_constant) uniform Push
{
    mat4 model;
    vec4 voxelGridMin;
    vec4 voxelGridSize;
} push;

void main()
{
    vec3 normalizedPos = (fragWorldPos - push.voxelGridMin.xyz) / push.voxelGridSize.xyz;
    ivec3 voxelCoord = ivec3(normalizedPos * float(imageSize(voxelTexture).x));

    if (any(lessThan(voxelCoord, ivec3(0))) || any(greaterThanEqual(voxelCoord, imageSize(voxelTexture))))
        return;

    vec3 albedo = texture(albedoMap, fragTexCoord).rgb;
    imageStore(voxelTexture, voxelCoord, vec4(albedo, 1.0));
}
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

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
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(push.model) * inNormal;
    fragTexCoord = inTexCoord;

    vec3 normalizedPos = (worldPos.xyz - push.voxelGridMin.xyz) / push.voxelGridSize.xyz;
    gl_Position = vec4(normalizedPos.xy * 2.0 - 1.0, normalizedPos.z, 1.0);
}
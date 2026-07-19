#version 450
#extension GL_GOOGLE_include_directive : require

layout(binding = 0) uniform MVP
{
    mat4 view;
    mat4 proj;
    vec4 camPos;
    vec4 lightPos;
    vec4 lightColor;
} ubo;

layout(push_constant) uniform Push
{
    mat4 model;
    mat4 normalMatrix;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out mat3 fragTBN;

void main()
{
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragTexCoord = inTexCoord;

    mat3 nm = mat3(push.normalMatrix);
    vec3 N = normalize(nm * inNormal);
    vec3 T = normalize(nm * inTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    fragTBN = mat3(T, B, N);
}
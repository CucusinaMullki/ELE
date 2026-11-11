#version 450

layout(push_constant) uniform Push
{
    mat4 lightViewProj;
    mat4 model;
    vec4 lightPos;
} push;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragWorldPos;

void main()
{
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = push.lightViewProj * worldPos;
}

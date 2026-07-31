#version 450

layout(binding = 0) uniform sampler2D godrayTexture;

layout(push_constant) uniform Push
{
    float intensity;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(texture(godrayTexture, fragUV).rgb * push.intensity, 1.0);
}
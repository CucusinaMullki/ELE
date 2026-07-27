#version 450

layout(binding = 0) uniform sampler2D currentColor;
layout(binding = 1) uniform sampler2D velocityBuffer;
layout(binding = 2) uniform sampler2D historyColor;

layout(push_constant) uniform Push {
    float blendFactor;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 current = texture(currentColor, fragUV).rgb;
    outColor = vec4(current, 1.0);   // просто copy текущего кадра, БЕЗ reprojection/history
}
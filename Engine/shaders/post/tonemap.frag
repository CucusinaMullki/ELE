#version 450
#extension GL_GOOGLE_include_directive : require

#include "tonemap.glsl"

layout(binding = 0) uniform sampler2D hdrBuffer;
layout(binding = 1) uniform sampler2D bloomBuffer;

layout(push_constant) uniform Push {
    float exposure;
    float contrast;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 hdrColor = texture(hdrBuffer, fragUV).rgb;
    vec3 bloomColor = texture(bloomBuffer, fragUV).rgb;

    vec3 combined = hdrColor + bloomColor * 0.65;
    combined *= push.exposure;

    vec3 mapped = tonemapACESSoft(combined, push.contrast);
    outColor = vec4(mapped, 1.0);
}
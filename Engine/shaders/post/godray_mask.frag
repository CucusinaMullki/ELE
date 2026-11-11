#version 450

layout(binding = 0) uniform sampler2D depthTexture;

layout(push_constant) uniform Push
{
    mat4 invViewProj;
    vec4 sunDirIntensity;
    vec4 sunColorSize;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    if (push.sunDirIntensity.w < 0.5)
    {
        outColor = vec4(0.0);
        return;
    }

    float sceneDepth = texture(depthTexture, fragUV).r;

    if (sceneDepth < 0.9999)
    {
        outColor = vec4(0.0);
        return;
    }

    vec2 ndcXY = fragUV * 2.0 - 1.0;
    vec4 nearH = push.invViewProj * vec4(ndcXY, 0.0, 1.0);
    vec4 farH  = push.invViewProj * vec4(ndcXY, 1.0, 1.0);
    vec3 dir = normalize(farH.xyz / farH.w - nearH.xyz / nearH.w);

    float cosAngle = dot(dir, push.sunDirIntensity.xyz);
    float sunSize = push.sunColorSize.a;
    float sunDisk = smoothstep(sunSize, sunSize + 0.0005, cosAngle);

    outColor = vec4(push.sunColorSize.rgb * sunDisk, 1.0);
}

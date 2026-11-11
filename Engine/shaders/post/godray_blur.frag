#version 450

layout(binding = 0) uniform sampler2D maskTexture;

layout(push_constant) uniform Push
{
    vec4 sunUVParams;
    vec4 weightCountParams;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 sunUV = push.sunUVParams.xy;
    float density = push.sunUVParams.z;
    float decay = push.sunUVParams.w;
    float weight = push.weightCountParams.x;
    int sampleCount = int(push.weightCountParams.y);

    vec2 deltaUV = (fragUV - sunUV) * density / float(sampleCount);

    vec2 uv = fragUV;
    float illuminationDecay = 1.0;
    vec3 result = vec3(0.0);

    for (int i = 0; i < sampleCount; i++)
    {
        uv -= deltaUV;
        vec3 sampleColor = texture(maskTexture, uv).rgb;
        sampleColor *= illuminationDecay * weight;
        result += sampleColor;
        illuminationDecay *= decay;
    }

    outColor = vec4(result, 1.0);
}

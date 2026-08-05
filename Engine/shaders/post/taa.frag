#version 450

layout(binding = 0) uniform sampler2D currentColor;
layout(binding = 1) uniform sampler2D velocityBuffer;
layout(binding = 2) uniform sampler2D historyColor;

layout(push_constant) uniform Push {
    float blendFactor;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

vec3 rgbToYCoCg(vec3 c)
{
    float y  = dot(c, vec3(0.25, 0.5, 0.25));
    float co = dot(c, vec3(0.5, 0.0, -0.5));
    float cg = dot(c, vec3(-0.25, 0.5, -0.25));
    return vec3(y, co, cg);
}

vec3 yCoCgToRgb(vec3 c)
{
    float y = c.x, co = c.y, cg = c.z;
    return vec3(y + co - cg, y + cg, y - co - cg);
}

void main()
{
    vec3 current = texture(currentColor, fragUV).rgb;

    vec2 velocity = texture(velocityBuffer, fragUV).rg;
    vec2 prevUV = fragUV - velocity;

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0)
    {
        outColor = vec4(current, 1.0);
        return;
    }

    vec3 history = texture(historyColor, prevUV).rgb;

    vec2 texelSize = 1.0 / vec2(textureSize(currentColor, 0));

    vec3 neighborhoodMin = vec3(1e6);
    vec3 neighborhoodMax = vec3(-1e6);

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            vec3 sampleColor = texture(currentColor, fragUV + vec2(x, y) * texelSize).rgb;
            vec3 sampleYCoCg = rgbToYCoCg(sampleColor);
            neighborhoodMin = min(neighborhoodMin, sampleYCoCg);
            neighborhoodMax = max(neighborhoodMax, sampleYCoCg);
        }
    }

    vec3 historyYCoCg = rgbToYCoCg(history);
    vec3 clampedYCoCg = clamp(historyYCoCg, neighborhoodMin, neighborhoodMax);
    vec3 clampedHistory = yCoCgToRgb(clampedYCoCg);

    vec3 result = mix(current, clampedHistory, push.blendFactor);
    outColor = vec4(result, 1.0);
}
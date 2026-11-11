#version 450

layout(binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform Push
{
    float radius;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texel = push.radius / vec2(textureSize(srcTexture, 0));

    vec3 result = texture(srcTexture, fragUV + texel * vec2(-1.0, -1.0)).rgb * 1.0;
    result += texture(srcTexture, fragUV + texel * vec2( 0.0, -1.0)).rgb * 2.0;
    result += texture(srcTexture, fragUV + texel * vec2( 1.0, -1.0)).rgb * 1.0;
    result += texture(srcTexture, fragUV + texel * vec2(-1.0,  0.0)).rgb * 2.0;
    result += texture(srcTexture, fragUV + texel * vec2( 0.0,  0.0)).rgb * 4.0;
    result += texture(srcTexture, fragUV + texel * vec2( 1.0,  0.0)).rgb * 2.0;
    result += texture(srcTexture, fragUV + texel * vec2(-1.0,  1.0)).rgb * 1.0;
    result += texture(srcTexture, fragUV + texel * vec2( 0.0,  1.0)).rgb * 2.0;
    result += texture(srcTexture, fragUV + texel * vec2( 1.0,  1.0)).rgb * 1.0;

    outColor = vec4(result / 16.0, 1.0);
}

#version 450

layout(binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform Push 
{
    vec2 scrTexelSize;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = fragUV;
    vec2 texel = push.scrTexelSize;

    vec3 a = texture(srcTexture, uv + texel * vec2(-1.0, -1.0)).rgb;
    vec3 b = texture(srcTexture, uv + texel * vec2( 0.0, -1.0)).rgb;
    vec3 c = texture(srcTexture, uv + texel * vec2( 1.0, -1.0)).rgb;
    vec3 d = texture(srcTexture, uv + texel * vec2(-0.5, -0.5)).rgb;
    vec3 e = texture(srcTexture, uv + texel * vec2( 0.5, -0.5)).rgb;
    vec3 f = texture(srcTexture, uv + texel * vec2(-1.0,  0.0)).rgb;
    vec3 g = texture(srcTexture, uv).rgb;
    vec3 h = texture(srcTexture, uv + texel * vec2( 1.0,  0.0)).rgb;
    vec3 i = texture(srcTexture, uv + texel * vec2(-0.5,  0.5)).rgb;
    vec3 j = texture(srcTexture, uv + texel * vec2( 0.5,  0.5)).rgb;
    vec3 k = texture(srcTexture, uv + texel * vec2(-1.0,  1.0)).rgb;
    vec3 l = texture(srcTexture, uv + texel * vec2( 0.0,  1.0)).rgb;
    vec3 m = texture(srcTexture, uv + texel * vec2( 1.0,  1.0)).rgb;

    vec3 result = (d + e + i + j) * 0.125;
    result += (a + b + f + g) * 0.03125;
    result += (b + c + g + h) * 0.03125;
    result += (f + g + k + l) * 0.03125;
    result += (g + h + l + m) * 0.03125;

    outColor = vec4(result, 1.0);    
}
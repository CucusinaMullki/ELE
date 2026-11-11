#ifndef TONEMAP_GLSL
#define TONEMAP_GLSL

vec3 tonemapReinhard(vec3 color)
{
    color = color / (1.0 + color);
    return pow(color, vec3(1.0 / 2.2));
}

vec3 tonemapACES(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    vec3 mapped = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
    return pow(mapped, vec3(1.0 / 2.2));
}
vec3 tonemapACESSoft(vec3 color, float contrast)
{
    vec3 filmic = tonemapACES(color);
    vec3 linear = clamp(pow(color, vec3(1.0 / 2.2)), 0.0, 1.0);
    return mix(linear, filmic, clamp(contrast, 0.0, 1.0));
}

#endif

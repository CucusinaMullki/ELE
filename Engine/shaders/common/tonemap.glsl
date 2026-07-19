#ifndef TONEMAP_GLSL
#define TONEMAP_GLSL

vec3 tonemapReinhard(vec3 color)
{
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    return color;
}

#endif
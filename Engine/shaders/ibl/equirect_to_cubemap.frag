#version 450

layout(location = 0) in vec3 fragLocalPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D equirectMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleSphericalMap(vec3 dir)
{
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}

void main()
{
    vec2 uv = sampleSphericalMap(normalize(fragLocalPos));
    vec3 color = texture(equirectMap, uv).rgb;
    outColor = vec4(color, 1.0);
}

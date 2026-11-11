#ifndef PROBE_DIRS_GLSL
#define PROBE_DIRS_GLSL

uint probeHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

vec3 sphericalFibonacci(float i, float n)
{
    float phi = 6.2831853 * fract(i * 0.61803398875);
    float z = 1.0 - (2.0 * i + 1.0) / n;
    float r = sqrt(max(0.0, 1.0 - z * z));
    return vec3(cos(phi) * r, sin(phi) * r, z);
}

mat3 randomRotation(vec3 u)
{
    float a = sqrt(1.0 - u.x), b = sqrt(u.x);
    float t1 = 6.2831853 * u.y, t2 = 6.2831853 * u.z;
    float x = a * sin(t1), y = a * cos(t1), z = b * sin(t2), w = b * cos(t2);
    return mat3(
        1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w),       2.0 * (x * z - y * w),
        2.0 * (x * y - z * w),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + x * w),
        2.0 * (x * z + y * w),       2.0 * (y * z - x * w),       1.0 - 2.0 * (x * x + y * y));
}

mat3 probeRotation(uint probeIdx, uint frameIndex)
{
    uint h = probeHash(probeIdx * 9781u + frameIndex * 6271u + 1u);
    vec3 u = vec3(float(probeHash(h)), float(probeHash(h + 1u)), float(probeHash(h + 2u))) / 4294967296.0;
    return randomRotation(u);
}

#endif

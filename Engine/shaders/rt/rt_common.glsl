layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

layout(set = 0, binding = 2) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    mat4 prevViewProj;
    mat4 viewProjNoJitter;
    vec4 camPos;
    vec4 iblParams;
} cam;

#include "light.glsl"
layout(set = 0, binding = 3) uniform LightUBO {
    Light lights[16];
    ivec4 count;
} lightData;

struct HitPayload { float t; vec3 normal; vec2 uv; vec3 albedo; uint objectIndex; uint materialIndex; };
layout(location = 0) rayPayloadEXT HitPayload pl;

const float HIT_AMBIENT = 0.04;
const int MAX_HIT_LIGHTS = 8;
const float SKY_STRENGTH = 0.0;

vec3 sky(vec3 dir)
{
    float t = smoothstep(0.0, 1.0, dir.y * 0.5 + 0.5);
    vec3 upper = mix(vec3(0.55, 0.60, 0.65), vec3(0.20, 0.32, 0.55), t);
    return mix(vec3(0.05, 0.05, 0.06), upper, smoothstep(-0.05, 0.05, dir.y));
}

bool occluded(vec3 o, vec3 d, float tmax)
{
    pl.t = 1.0;
    traceRayEXT(tlas,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF, 0, 0, 0, o, 0.02, d, tmax, 0);
    return pl.t > 0.0;
}

vec3 shadeHit(vec3 P, vec3 N, vec3 albedo)
{
    vec3 lin = pow(albedo, vec3(2.2));
    vec3 Lo = vec3(0.0);
    vec3 o = P + N * 0.02;
    int n = min(lightData.count.x, MAX_HIT_LIGHTS);
    for (int i = 0; i < n; ++i)
    {
        Light l = lightData.lights[i];
        vec3 L, radiance;
        float tmax;
        if (l.position.w < 0.5) {
            L = normalize(-l.position.xyz);
            radiance = l.color.rgb * l.color.a;
            tmax = 10000.0;
        } else {
            vec3 toL = l.position.xyz - P;
            float d2 = max(dot(toL, toL), 0.01);
            L = normalize(toL);
            radiance = l.color.rgb * l.color.a / d2;
            tmax = sqrt(d2) - 0.05;
        }
        float NdotL = dot(N, L);
        if (NdotL <= 0.0 || max(radiance.r, max(radiance.g, radiance.b)) * NdotL < 0.002) continue;
        if (occluded(o, L, tmax)) continue;
        Lo += lin / 3.14159265 * radiance * NdotL;
    }
    return lin * HIT_AMBIENT + Lo;
}

uint pcg(uint v)
{
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}

vec2 rand2(ivec2 px, uint frame)
{
    uint h = pcg(uint(px.x) + pcg(uint(px.y) + pcg(frame)));
    uint h2 = pcg(h);
    return vec2(float(h), float(h2)) / 4294967296.0;
}

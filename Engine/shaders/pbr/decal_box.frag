#version 450
#extension GL_GOOGLE_include_directive : require
#include "brdf.glsl"

layout(binding = 0) uniform sampler2D decalTexture;
layout(binding = 1) uniform sampler2D depthTexture;

layout(binding = 2) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    vec4 camPos;
} cam;

struct Light { vec4 position; vec4 color; };
layout(binding = 3) uniform LightUBO {
    Light lights[16];
    ivec4 count;
} lightData;

layout(push_constant) uniform Push {
    mat4 model;
    mat4 invModel;
    vec4 params; // x=metallic, y=roughness
} push;

layout(location = 0) in vec4 fragClipPos;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 screenUV = (fragClipPos.xy / fragClipPos.w) * 0.5 + 0.5;
    float sceneDepth = texture(depthTexture, screenUV).r;

    vec4 ndc = vec4(screenUV * 2.0 - 1.0, sceneDepth, 1.0);
    vec4 worldPosH = cam.invViewProj * ndc;
    vec3 worldPos = worldPosH.xyz / worldPosH.w;

    vec3 localPos = (push.invModel * vec4(worldPos, 1.0)).xyz;
    if (any(greaterThan(abs(localPos), vec3(0.5))))
        discard;

    vec2 decalUV = localPos.xz + 0.5;
    vec4 tex = texture(decalTexture, decalUV);
    if (tex.a < 0.01)
        discard;

    vec3 albedo = pow(tex.rgb, vec3(2.2));

    // нормаль = локальная ось Y бокса, повёрнутая в мир (без учёта реальной геометрии сцены)
    vec3 N = normalize((push.model * vec4(0.0, 1.0, 0.0, 0.0)).xyz);

    float metallic  = push.params.x;
    float roughness = clamp(push.params.y, 0.05, 1.0);

    vec3 V = normalize(cam.camPos.xyz - worldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightData.count.x; i++)
    {
        Light light = lightData.lights[i];
        vec3 L, radiance;

        if (light.position.w < 0.5) {
            L = normalize(-light.position.xyz);
            radiance = light.color.rgb * light.color.a;
        } else {
            vec3 toLight = light.position.xyz - worldPos;
            float dist2 = max(dot(toLight, toLight), 0.01);
            L = normalize(toLight);
            radiance = light.color.rgb * light.color.a / dist2;
        }

        vec3 H = normalize(V + L);
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.02) * albedo;
    outColor = vec4(ambient + Lo, tex.a);
}
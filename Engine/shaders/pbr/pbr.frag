#version 450
#extension GL_GOOGLE_include_directive : require

#include "brdf.glsl"
#include "tonemap.glsl"

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 camPos;
} cam;

layout(binding = 1) uniform sampler2D albedoMap;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D metallicRoughnessMap;
layout(binding = 4) uniform sampler2D aoMap;

struct Light {
    vec4 position;
    vec4 color;
};

layout(binding = 5) uniform LightUBO {
    Light lights[16];
    ivec4 count;
} lightData;

layout(binding = 6) uniform sampler2DShadow shadowMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragLightSpacePos;
layout(location = 3) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

const vec3 SKY_ZENITH  = vec3(0.20, 0.32, 0.55);
const vec3 SKY_HORIZON = vec3(0.55, 0.60, 0.65);
const vec3 GROUND      = vec3(0.05, 0.05, 0.06);

vec3 sampleSky(vec3 dir)
{
    float t = smoothstep(0.0, 1.0, dir.y * 0.5 + 0.5);
    vec3 upperSky = mix(SKY_HORIZON, SKY_ZENITH, t);
    return mix(GROUND, upperSky, smoothstep(-0.05, 0.05, dir.y));
}

float interleavedGradientNoise(vec2 p)
{
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(p, magic.xy)));
}

float calculateShadow(vec4 lightSpacePos, vec3 N, vec3 L, vec2 screenXY)
{
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z < 0.0 || proj.z > 1.0) {
        return 1.0;
    }

    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0006);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    float angle = interleavedGradientNoise(screenXY) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    float shadow = 0.0;
    const int R = 2; // 5x5 kernel, rotated per pixel -> soft penumbra
    for (int x = -R; x <= R; x++) {
        for (int y = -R; y <= R; y++) {
            vec2 offset = rot * (vec2(x, y) * texelSize);
            shadow += texture(shadowMap, vec3(proj.xy + offset, proj.z - bias));
        }
    }
    float samples = float((2 * R + 1) * (2 * R + 1));
    return shadow / samples;
}

void main()
{
    vec3 albedo = pow(texture(albedoMap, fragTexCoord).rgb, vec3(2.2));
    vec3 mr = texture(metallicRoughnessMap, fragTexCoord).rgb;
    float roughness = clamp(mr.g, 0.05, 1.0);
    float metallic = mr.b;
    float ao = texture(aoMap, fragTexCoord).r;

    vec3 N = texture(normalMap, fragTexCoord).rgb;
    N = N * 2.0 - 1.0;
    N = normalize(fragTBN * N);

    vec3 V = normalize(cam.camPos.xyz - fragWorldPos);
    vec3 R = reflect(-V, N);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightData.count.x; i++) {
        Light light = lightData.lights[i];
        vec3 L, radiance;

        if (light.position.w < 0.5) {
            L = normalize(-light.position.xyz);
            radiance = light.color.rgb * light.color.a;
        } else {
            vec3 toLight = light.position.xyz - fragWorldPos;
            float dist = length(toLight);
            L = normalize(toLight);
            float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
            radiance = light.color.rgb * light.color.a * atten;
        }

        vec3 H = normalize(V + L);
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);

        float visibility = 1.0;
        if (i == 0 && light.position.w < 0.5) {
            visibility = calculateShadow(fragLightSpacePos, N, L, gl_FragCoord.xy);
        }

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * visibility;
    }
    vec3 kS_ambient = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD_ambient = (1.0 - kS_ambient) * (1.0 - metallic);

    vec3 irradiance = sampleSky(N);
    vec3 diffuseIBL = irradiance * albedo * kD_ambient;

    vec3 prefiltered = mix(sampleSky(R), irradiance, roughness * roughness);
    vec3 specularIBL = prefiltered * kS_ambient;

    vec3 ambient = (diffuseIBL + specularIBL) * ao * 1.8;

    vec3 color = ambient + Lo;

    outColor = vec4(color, 1.0);
}
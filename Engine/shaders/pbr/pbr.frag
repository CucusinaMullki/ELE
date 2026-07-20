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

float calculateShadow(vec4 lightSpacePos, vec3 N, vec3 L)
{
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 offset = vec2(x, y) * texelSize;
            float pcf = texture(shadowMap, vec3(proj.xy + offset, proj.z - bias));
            shadow += pcf;
        }
    }
    shadow /= 9.0;

    return 1.0 - shadow;
}

void main()
{
    vec3 albedo = pow(texture(albedoMap, fragTexCoord).rgb, vec3(2.2));
    vec3 mr = texture(metallicRoughnessMap, fragTexCoord).rgb;
    float roughness = mr.g;
    float metallic = mr.b;
    float ao = texture(aoMap, fragTexCoord).r;

    vec3 N = texture(normalMap, fragTexCoord).rgb;
    N = N * 2.0 - 1.0;
    N = normalize(fragTBN * N);

    vec3 V = normalize(cam.camPos.xyz - fragWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightData.count.x; i++)
    {
        Light light = lightData.lights[i];

        vec3 L;
        vec3 radiance;

        if (light.position.w < 0.5)
        {
            L = normalize(-light.position.xyz);
            radiance = light.color.rgb * light.color.a;
        }
        else
        {
            vec3 toLight = light.position.xyz - fragWorldPos;
            float dist = length(toLight);
            L = normalize(toLight);
            float attenuation = 1.0 / (dist * dist);
            radiance = light.color.rgb * light.color.a * attenuation;
        }

        vec3 H = normalize(V + L);

        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 mainL = normalize(-lightData.lights[0].position.xyz);
    float shadow = calculateShadow(fragLightSpacePos, N, mainL);
    Lo *= (1.0 - shadow);

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    color = tonemapReinhard(color);
    outColor = vec4(color, 1.0);
}
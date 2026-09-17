#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D albedoMap;
layout(binding = 1, rgba8) uniform image3D voxelTexture;

struct Light
{
    vec4 position;
    vec4 color;
};

layout(binding = 2) uniform LightUBO
{
    Light lights[16];
    ivec4 count;
} lightData;

layout(binding = 3) uniform sampler2DShadow shadowMap;
layout(binding = 4) uniform sampler2D shadowAtlas;

layout(binding = 5) uniform ShadowData
{
    mat4 lightSpaceMatrix;
} shadowData;

layout(push_constant) uniform Push
{
    mat4 model;
    vec4 voxelGridMin;
    vec4 voxelGridSize;
    ivec4 atlasOffset;
} push;

float calculateShadow(vec3 worldPos, vec3 N, vec3 L)
{
    vec4 lightSpacePos = shadowData.lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z < 0.0 || proj.z > 1.0)
        return 1.0;

    float bias = max(0.003 * (1.0 - dot(N, L)), 0.001);
    return texture(shadowMap, vec3(proj.xy, proj.z - bias));
}

void main()
{
    vec3 normalizedPos = (fragWorldPos - push.voxelGridMin.xyz) / push.voxelGridSize.xyz;
    int chunkResolution = push.atlasOffset.w;
    ivec3 localVoxelCoord = ivec3(normalizedPos * float(chunkResolution));

    if (any(lessThan(localVoxelCoord, ivec3(0))) || any(greaterThanEqual(localVoxelCoord, ivec3(chunkResolution))))
        return;

    ivec3 voxelCoord = push.atlasOffset.xyz + localVoxelCoord;

    vec3 albedo = texture(albedoMap, fragTexCoord).rgb;
    vec3 N = normalize(fragNormal);

    vec3 result = vec3(0.0);

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
            float dist2 = max(dot(toLight, toLight), 0.01);
            L = normalize(toLight);
            radiance = light.color.rgb * light.color.a / dist2;
        }

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        float visibility = 1.0;
        if (i == 0 && light.position.w < 0.5)
            visibility = calculateShadow(fragWorldPos, N, L);

        result += albedo * radiance * NdotL * visibility;
    }

    float maxBrightness = 4.0;
    float luminance = dot(result, vec3(0.2126, 0.7152, 0.0722));
    if (luminance > maxBrightness)
        result *= maxBrightness / luminance;

    imageStore(voxelTexture, voxelCoord, vec4(result, 1.0));
}
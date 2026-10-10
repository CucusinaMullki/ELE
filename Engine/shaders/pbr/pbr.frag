#version 450
#extension GL_GOOGLE_include_directive : require

#include "brdf.glsl"
#include "tonemap.glsl"

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    mat4 prevViewProj;
    mat4 viewProjNoJitter;
    vec4 camPos;
    vec4 iblParams;
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
layout(binding = 7) uniform sampler2D shadowAtlas;
layout(binding = 8) uniform sampler2D lightmapMap;
layout(binding = 9) uniform samplerCube irradianceMap;
layout(binding = 10) uniform sampler3D voxelTexture;

layout(binding = 11) readonly buffer PageTable {
    int slots[];
} pageTable;

layout(binding = 12) uniform VoxelGridInfo {
    vec4 chunkParams;
    ivec4 cameraChunkCoord;
    vec4 flags;
} voxelInfo;

layout(binding = 13) readonly buffer ProbeData { float probeData[]; };
layout(binding = 14) uniform ProbeInfo
{
    vec4  originSpacing;
    uvec4 dims;
} probeInfo;

const float PROBE_STRENGTH = 0.5;
const float PROBE_NORMAL_BIAS = 0.3;
const float IBL_WITH_PROBES = 0.3;
const float AMBIENT_BLACK = 0.002;
const float AMBIENT_CONTRAST = 1.25;

vec3 shapeAmbient(vec3 a)
{
    a = max(a - vec3(AMBIENT_BLACK), vec3(0.0));
    return pow(a, vec3(AMBIENT_CONTRAST));
}

vec3 probeIrradianceOne(uint idx, vec3 n)
{
    uint base = idx * 28u;
    float Y[9];
    Y[0] = 0.282095;
    Y[1] = 0.488603 * n.y;
    Y[2] = 0.488603 * n.z;
    Y[3] = 0.488603 * n.x;
    Y[4] = 1.092548 * n.x * n.y;
    Y[5] = 1.092548 * n.y * n.z;
    Y[6] = 0.315392 * (3.0 * n.z * n.z - 1.0);
    Y[7] = 1.092548 * n.x * n.z;
    Y[8] = 0.546274 * (n.x * n.x - n.y * n.y);

    vec3 c = vec3(0.0);
    for (uint i = 0u; i < 9u; ++i)
        c += vec3(probeData[base + i*3u], probeData[base + i*3u + 1u], probeData[base + i*3u + 2u]) * Y[i];
    return max(c, vec3(0.0));
}

vec4 sampleProbes(vec3 worldPos, vec3 n)
{
    float spacing = probeInfo.originSpacing.w;
    vec3  origin  = probeInfo.originSpacing.xyz;
    ivec3 d = ivec3(probeInfo.dims.xyz);

    vec3 biasedPos = worldPos + n * (spacing * PROBE_NORMAL_BIAS);
    vec3 g = clamp((biasedPos - origin) / spacing, vec3(0.0), vec3(d - 1) - 1e-3);
    ivec3 i0 = ivec3(floor(g));
    vec3  f  = g - vec3(i0);

    vec3  sum = vec3(0.0);
    float wsum = 0.0;
    for (int k = 0; k < 8; ++k)
    {
        ivec3 o = ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
        ivec3 p = min(i0 + o, d - 1);
        uint  idx = uint(p.x + d.x * (p.y + d.y * p.z));
        float valid = probeData[idx * 28u + 27u];
        if (valid < 0.5) continue;

        vec3  probePos = origin + vec3(p) * spacing;
        vec3  toProbe  = probePos - worldPos;
        float len = length(toProbe);
        float facing = len > 1e-3 ? (dot(toProbe / len, n) + 1.0) * 0.5 : 1.0;
        facing = facing * facing + 0.2;

        vec3  t = mix(1.0 - f, f, vec3(o));
        float w = t.x * t.y * t.z * facing;
        if (w > 0.0) { sum += probeIrradianceOne(idx, n) * w; wsum += w; }
    }
    vec3  gRaw = (worldPos - origin) / spacing;
    vec3  edge3 = min(gRaw, vec3(d - 1) - gRaw);
    float edge = min(edge3.x, min(edge3.y, edge3.z));
    float fade = smoothstep(-1.0, 0.0, edge);

    return wsum > 1e-4 ? vec4(sum / wsum * PROBE_STRENGTH, fade) : vec4(0.0);
}

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragLightSpacePos;
layout(location = 3) in mat3 fragTBN;

layout(location = 6) in vec4 fragCurrClipPos;
layout(location = 7) in vec4 fragPrevClipPos;
layout(location = 8) in vec2 fragLightmapUV;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outVelocity;

const vec3 SKY_ZENITH  = vec3(0.20, 0.32, 0.55);
const vec3 SKY_HORIZON = vec3(0.55, 0.60, 0.65);
const vec3 GROUND      = vec3(0.05, 0.05, 0.06);

const float POINT_SHADOW_FAR_PLANE = 25.0;

vec3 sampleAtlasVoxel(vec3 worldPos)
{
    float chunkSize = voxelInfo.chunkParams.x;
    int chunkResolution = int(voxelInfo.chunkParams.y);
    int slotsPerAxis = int(voxelInfo.chunkParams.z);
    int radius = int(voxelInfo.chunkParams.w);
    int paddedResolution = voxelInfo.cameraChunkCoord.w;

    ivec3 chunkCoord = ivec3(floor(worldPos / chunkSize));
    ivec3 relCoord = chunkCoord - voxelInfo.cameraChunkCoord.xyz + ivec3(radius);

    if (any(lessThan(relCoord, ivec3(0))) || any(greaterThanEqual(relCoord, ivec3(slotsPerAxis))))
        return vec3(0.0);

    int pageIndex = relCoord.x + relCoord.y * slotsPerAxis + relCoord.z * slotsPerAxis * slotsPerAxis;
    int atlasSlot = pageTable.slots[pageIndex];

    if (atlasSlot < 0)
        return vec3(0.0);

    int slotZ = atlasSlot / (slotsPerAxis * slotsPerAxis);
    int slotY = (atlasSlot / slotsPerAxis) % slotsPerAxis;
    int slotX = atlasSlot % slotsPerAxis;

    vec3 chunkWorldMin = vec3(chunkCoord) * chunkSize;
    vec3 localPos = (worldPos - chunkWorldMin) / chunkSize;

    float paddingFrac = 1.0 / float(paddedResolution);
    vec3 paddedLocalPos = paddingFrac + localPos * (1.0 - 2.0 * paddingFrac);

    vec3 atlasUV = (vec3(slotX, slotY, slotZ) + paddedLocalPos) / float(slotsPerAxis);

    return texture(voxelTexture, atlasUV).rgb;
}

vec3 calculateVoxelGI(vec3 worldPos, vec3 N)
{
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    const int NUM_CONES = 9;
    vec3 coneDirs[NUM_CONES] = vec3[](
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 0.866, 0.5),
        vec3(0.823, 0.267, 0.5),
        vec3(0.509, -0.7, 0.5),
        vec3(-0.509, -0.7, 0.5),
        vec3(-0.823, 0.267, 0.5),
        vec3(0.588, 0.588, 0.555),
        vec3(-0.588, 0.588, 0.555),
        vec3(0.0, -0.809, 0.588)
    );
    float coneWeights[NUM_CONES] = float[](0.16, 0.10, 0.10, 0.10, 0.10, 0.10, 0.11, 0.11, 0.12);

    vec3 result = vec3(0.0);
    float chunkSize = voxelInfo.chunkParams.x;
    float voxelSize = chunkSize / voxelInfo.chunkParams.y;

    for (int i = 0; i < NUM_CONES; i++)
    {
        vec3 dir = coneDirs[i].x * right + coneDirs[i].y * up + coneDirs[i].z * N;

        float dist = voxelSize * 0.5;
        float maxDist = chunkSize * 1.2;

        for (int step = 0; step < 30; step++)
        {
            if (dist > maxDist) break;

            vec3 samplePos = worldPos + dir * dist;
            vec3 s = sampleAtlasVoxel(samplePos);

            if (length(s) > 0.001)
            {
                vec3 sNext = sampleAtlasVoxel(worldPos + dir * (dist + voxelSize));
                vec3 blended = (length(sNext) > 0.001) ? (s + sNext) * 0.5 : s;
                result += blended * coneWeights[i];
                break;
            }

            dist += voxelSize * 0.5f;
        }
    }

    return result;
}

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
        proj.z < 0.0 || proj.z > 1.0)
    {
        return 1.0;
    }

    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0006);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    float angle = interleavedGradientNoise(screenXY) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    float shadow = 0.0;
    const int R = 1;
    for (int x = -R; x <= R; x++)
    {
        for (int y = -R; y <= R; y++)
        {
            vec2 offset = rot * (vec2(x, y) * texelSize);
            shadow += texture(shadowMap, vec3(proj.xy + offset, proj.z - bias));
        }
    }
    float samples = float((2 * R + 1) * (2 * R + 1));
    return shadow / samples;
}

vec3 sampleCubeFaceAndUV(vec3 dir)
{
    vec3 absDir = abs(dir);
    float faceIndex;
    vec2 uv;

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        faceIndex = dir.x > 0.0 ? 0.0 : 1.0;
        uv = (dir.x > 0.0) ? vec2(-dir.z, -dir.y) / absDir.x : vec2(dir.z, -dir.y) / absDir.x;
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
    {
        faceIndex = dir.y > 0.0 ? 2.0 : 3.0;
        uv = (dir.y > 0.0) ? vec2(dir.x, dir.z) / absDir.y : vec2(dir.x, -dir.z) / absDir.y;
    }
    else
    {
        faceIndex = dir.z > 0.0 ? 4.0 : 5.0;
        uv = (dir.z > 0.0) ? vec2(dir.x, -dir.y) / absDir.z : vec2(-dir.x, -dir.y) / absDir.z;
    }

    uv = uv * 0.5 + 0.5;
    return vec3(uv, faceIndex);
}

float sampleShadowAtlas(vec3 dir, int lightSlot)
{
    vec3 faceUV = sampleCubeFaceAndUV(dir);

    const float TILE = 1.0 / 6.0;
    const float ROW = 1.0 / 3.0;

    vec2 atlasUV;
    atlasUV.x = (faceUV.z + faceUV.x) * TILE;
    atlasUV.y = (float(lightSlot) + faceUV.y) * ROW;

    return texture(shadowAtlas, atlasUV).r;
}

float calculatePointShadow(vec3 fragPos, vec3 lightPos, vec3 N, vec2 screenXY, int shadowSlot)
{
    vec3 toFrag = fragPos - lightPos;
    float currentDist = length(toFrag);
    vec3 L = normalize(-toFrag);

    float bias = max(0.15 * (1.0 - dot(N, L)), 0.03);

    const int SAMPLES = 20;
    vec3 sampleOffsets[SAMPLES] = vec3[]
    (
        vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
        vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
        vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
        vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
        vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );

    float angle = interleavedGradientNoise(screenXY) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    vec3 axis = L;

    mat3 rot = mat3(
        c + axis.x*axis.x*(1.0-c),        axis.x*axis.y*(1.0-c) - axis.z*s, axis.x*axis.z*(1.0-c) + axis.y*s,
        axis.y*axis.x*(1.0-c) + axis.z*s, c + axis.y*axis.y*(1.0-c),        axis.y*axis.z*(1.0-c) - axis.x*s,
        axis.z*axis.x*(1.0-c) - axis.y*s, axis.z*axis.y*(1.0-c) + axis.x*s, c + axis.z*axis.z*(1.0-c)
    );

    float shadow = 0.0;
    float diskRadius = (1.0 + (currentDist / POINT_SHADOW_FAR_PLANE)) / 45.0;

    for (int i = 0; i < SAMPLES; i++)
    {
        vec3 offset = rot * sampleOffsets[i];
        float closestDist = sampleShadowAtlas(toFrag + offset * diskRadius, shadowSlot) * POINT_SHADOW_FAR_PLANE;

        if (currentDist - bias > closestDist)
            shadow += 1.0;
    }

    return shadow / float(SAMPLES);
}

void main()
{
    vec3 albedo = pow(texture(albedoMap, fragTexCoord).rgb, vec3(2.2));
    vec3 mr = texture(metallicRoughnessMap, fragTexCoord).rgb;
    float roughness = clamp(mr.g, 0.4, 1.0);
    float metallic = mr.b;
    float ao = texture(aoMap, fragTexCoord).r;

    vec3 N = texture(normalMap, fragTexCoord).rgb;
    N = N * 2.0 - 1.0;
    N = normalize(fragTBN * N);

    vec3 V = normalize(cam.camPos.xyz - fragWorldPos);
    vec3 R = reflect(-V, N);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightData.count.x; i++)
    {
        Light light = lightData.lights[i];
        vec3 L, radiance;

        if (light.position.w < 0.5) {
            L = normalize(-light.position.xyz);
            radiance = light.color.rgb * light.color.a;
        }
        else
        {
            vec3 toLight = light.position.xyz - fragWorldPos;
            float dist2 = max(dot(toLight, toLight), 0.01);
            L = normalize(toLight);
            float atten = 1.0 / dist2;
            radiance = light.color.rgb * light.color.a * atten;

            int shadowSlot = -1;
            if (i == lightData.count.y) shadowSlot = 0;
            else if (i == lightData.count.z) shadowSlot = 1;
            else if (i == lightData.count.w) shadowSlot = 2;

            if (shadowSlot >= 0)
            {
                float pShadow = calculatePointShadow(fragWorldPos, light.position.xyz, N, gl_FragCoord.xy, shadowSlot);
                radiance *= (1.0 - pShadow);
            }
        }

        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);

        if (NdotL > 0.0)
        {
            vec3 H = normalize(V + L);
            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 numerator = D * G * F;
            float denominator = 4.0 * NdotV * NdotL + 0.0001;
            vec3 specular = numerator / denominator;
            specular = min(specular, vec3(8.0));

            vec3 kD = (1.0 - F) * (1.0 - metallic);

            float visibility = 1.0;
            if (i == 0 && light.position.w < 0.5) {
                vec3 geoN = normalize(fragTBN[2]);
                visibility = calculateShadow(fragLightSpacePos, geoN, L, gl_FragCoord.xy);
            }

            Lo += (kD * albedo / PI + specular) * radiance * NdotL * visibility;
        }
    }

    //vec3 irradiance = texture(irradianceMap, N).rgb;
    //vec3 ambientDiffuse = irradiance * albedo * (1.0 - metallic) * cam.iblParams.x;
    //vec3 ambient = ambientDiffuse * ao;
    
    vec3 iblAmbient = texture(irradianceMap, N).rgb * albedo * (1.0 - metallic) * cam.iblParams.x;

    vec3 indirectDiffuse = iblAmbient;
    if (probeInfo.dims.w != 0u)
    {
        vec4 pr = sampleProbes(fragWorldPos, N);
        vec3 probeAmbient = pr.rgb * albedo * (1.0 - metallic);
        vec3 inside = probeAmbient + iblAmbient * IBL_WITH_PROBES;
        indirectDiffuse = mix(iblAmbient, inside, pr.a);
    }

    vec3 ambient = shapeAmbient(indirectDiffuse) * ao;
    
    vec3 color = ambient + Lo;

    vec3 lightmapColor = texture(lightmapMap, fragLightmapUV).rgb;
    color += lightmapColor * 0.05 * ao;
    
    //vec3 rawVoxel = sampleAtlasVoxel(fragWorldPos);
    //outColor = vec4(rawVoxel * 2.0, 1.0);
    //return;

    //if (probeInfo.dims.w != 0u) { outColor = vec4(sampleProbes(fragWorldPos, N).rgb, 0.05); return; }

    float alpha = texture(albedoMap, fragTexCoord).a;
    outColor = vec4(color, alpha);

    vec2 currNDC = fragCurrClipPos.xy / fragCurrClipPos.w;
    vec2 prevNDC = fragPrevClipPos.xy / fragPrevClipPos.w;
    outVelocity = (currNDC - prevNDC) * 0.5;
}
#version 450

layout(binding = 0) uniform sampler2D spriteTex;
layout(binding = 1) uniform sampler2D depthTex;

layout(binding = 2) uniform Camera
{
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    vec4 camPos;
} cam;

layout(std430, binding = 5) readonly buffer ProbeData { float probeData[]; };
layout(binding = 6) uniform ProbeInfo
{
    vec4  originSpacing;
    uvec4 dims;
} probeInfo;

layout(location = 0) in vec4  vColor;
layout(location = 1) in vec2  vUV;
layout(location = 2) in float vViewDepth;
layout(location = 3) flat in float vSoft;
layout(location = 4) in vec3  vWorld;
layout(location = 5) flat in float vProbe;

layout(location = 0) out vec4 outColor;

const float PROBE_NORMAL_BIAS = 0.3;
const float PARTICLE_PROBE_GAIN = 1.0;

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

    return wsum > 1e-4 ? vec4(sum / wsum, fade) : vec4(0.0);
}

void main()
{
    vec2  suv = gl_FragCoord.xy / vec2(textureSize(depthTex, 0));
    float sd  = texture(depthTex, suv).r;
    vec4  wp  = cam.invViewProj * vec4(suv * 2.0 - 1.0, sd, 1.0);
    vec3  sceneWorld = wp.xyz / wp.w;
    float sceneDepth = -(cam.view * vec4(sceneWorld, 1.0)).z;

    float diff = sceneDepth - vViewDepth;
    if (diff <= 0.0) discard;
    float fade = (vSoft > 0.0) ? clamp(diff / vSoft, 0.0, 1.0) : 1.0;

    vec4  tex = texture(spriteTex, vUV);
    float a   = tex.a * vColor.a * fade;
    if (a < 0.002) discard;
    vec3 rgb = tex.rgb * vColor.rgb;

    if (vProbe > 0.0 && probeInfo.dims.w != 0u)
    {
        vec3 n = normalize(cam.camPos.xyz - vWorld);
        vec4 pr = sampleProbes(vWorld, n);
        vec3 lit = mix(vec3(1.0), pr.rgb * PARTICLE_PROBE_GAIN, pr.a * vProbe);
        rgb *= lit;
    }

    outColor = vec4(rgb, a);
}
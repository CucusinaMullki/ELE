#version 450
#extension GL_GOOGLE_include_directive : require

#define EFFECT_BINDING 4
#include "particle_record.glsl"

layout(std430, binding = 3) readonly buffer InstanceBuffer { ParticleInstance instances[]; };

layout(binding = 2) uniform Camera
{
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    vec4 camPos;
} cam;

layout(push_constant) uniform Push
{
    uvec4 info;
} pc;

layout(location = 0) out vec4  vColor;
layout(location = 1) out vec2  vUV;
layout(location = 2) out float vViewDepth;
layout(location = 3) flat out float vSoft;
layout(location = 4) out vec3  vWorld;
layout(location = 5) flat out float vProbe;

const vec2 QUAD[6] = vec2[6](
    vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0),
    vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0));

void main()
{
    uint slot   = uint(gl_VertexIndex) / 6u;
    uint corner = uint(gl_VertexIndex) % 6u;
    ParticleInstance ins = instances[pc.info.y + slot];

    vColor = vec4(0.0); vUV = vec2(0.0); vViewDepth = 0.0; vSoft = 0.0; vWorld = vec3(0.0); vProbe = 0.0;
    if (ins.misc.z < 0.5 || ins.color.a <= 0.001)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    uint rec = pc.info.x * REC_WORDS;
    uint flags = ru(rec, O_FLAGS);

    vec3 right = vec3(cam.view[0][0], cam.view[1][0], cam.view[2][0]);
    vec3 up    = vec3(cam.view[0][1], cam.view[1][1], cam.view[2][1]);

    vec2  q = QUAD[corner];
    float c = cos(ins.misc.x), s = sin(ins.misc.x);
    vec2  l = vec2(c * q.x - s * q.y, s * q.x + c * q.y);
    vec3  world = ins.posSize.xyz + (right * l.x + up * l.y) * (ins.posSize.w * 0.5);

    uint cols = max(ru(rec, O_FLIPC), 1u);
    uint rows = max(ru(rec, O_FLIPR), 1u);
    uint frame = uint(ins.misc.y);
    vec2 cell = vec2(float(frame % cols), float((frame / cols) % rows));
    vec2 uv = vec2(q.x * 0.5 + 0.5, 0.5 - q.y * 0.5);
    vUV = (uv + cell) / vec2(float(cols), float(rows));

    vec4 viewPos = cam.view * vec4(world, 1.0);
    vViewDepth = -viewPos.z;
    vColor = ins.color;
    vSoft = ((flags & PFLAG_SOFT) != 0u) ? max(rf(rec, O_SOFT), 1e-3) : 0.0;
    vProbe = ((flags & PFLAG_PROBES) != 0u) ? clamp(rf(rec, O_PROBEINF), 0.0, 1.0) : 0.0;
    vWorld = ins.posSize.xyz;
    gl_Position = cam.proj * viewPos;
}

#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

struct HitPayload { float t; vec3 normal; vec2 uv; vec3 albedo; uint objectIndex; uint materialIndex; };
layout(location = 0) rayPayloadInEXT HitPayload pl;
hitAttributeEXT vec2 attribs;

layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer VertexBuf { float v[]; };
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer IndexBuf  { uint  i[]; };

struct GeoEntry { uvec2 vertexAddr; uvec2 indexAddr; uint firstIndex; uint materialIndex; uint objectIndex; uint pad; };
layout(set = 0, binding = 4, std430) readonly buffer GeoTable { GeoEntry geo[]; };

layout(set = 0, binding = 5) uniform sampler2D albedoTex[];
layout(set = 0, binding = 6) uniform sampler2D normalTex[];

vec3 nrm(VertexBuf vb, uint idx) { uint o = idx * 16u + 8u; return vec3(vb.v[o], vb.v[o + 1u], vb.v[o + 2u]); }
vec3 tng(VertexBuf vb, uint idx) { uint o = idx * 16u + 11u; return vec3(vb.v[o], vb.v[o + 1u], vb.v[o + 2u]); }
vec2 uvOf(VertexBuf vb, uint idx) { uint o = idx * 16u + 6u; return vec2(vb.v[o], vb.v[o + 1u]); }

void main()
{
    GeoEntry e = geo[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    VertexBuf vb = VertexBuf(e.vertexAddr);
    IndexBuf  ib = IndexBuf(e.indexAddr);

    uint base = e.firstIndex + uint(gl_PrimitiveID) * 3u;
    uint i0 = ib.i[base], i1 = ib.i[base + 1u], i2 = ib.i[base + 2u];

    vec3 bc = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 nObj = normalize(nrm(vb, i0) * bc.x + nrm(vb, i1) * bc.y + nrm(vb, i2) * bc.z);
    vec3 tRaw = tng(vb, i0) * bc.x + tng(vb, i1) * bc.y + tng(vb, i2) * bc.z;
    vec2 uv   = uvOf(vb, i0) * bc.x + uvOf(vb, i1) * bc.y + uvOf(vb, i2) * bc.z;

    vec3 wng = normalize(vec3(nObj * gl_WorldToObjectEXT));
    bool flip = dot(wng, gl_WorldRayDirectionEXT) > 0.0;

    vec3 nShade = nObj;
    float tLen = length(tRaw);
    if (tLen > 1e-4)
    {
        vec3 T = normalize(tRaw - nObj * dot(nObj, tRaw));
        vec3 B = cross(nObj, T);
        vec3 tn = textureLod(normalTex[nonuniformEXT(e.materialIndex)], uv, 0.0).rgb * 2.0 - 1.0;
        nShade = normalize(T * tn.x + B * tn.y + nObj * tn.z);
    }

    vec3 wn = normalize(vec3(nShade * gl_WorldToObjectEXT));
    if (flip) wn = -wn;

    pl.t = gl_HitTEXT;
    pl.normal = wn;
    pl.uv = uv;
    pl.objectIndex = e.objectIndex;
    pl.materialIndex = e.materialIndex;
    pl.albedo = textureLod(albedoTex[nonuniformEXT(e.materialIndex)], uv, 0.0).rgb;
}
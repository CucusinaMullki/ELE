#version 460
#extension GL_EXT_ray_tracing : require

struct HitPayload { float t; vec3 normal; vec2 uv; vec3 albedo; uint objectIndex; uint materialIndex; };
layout(location = 0) rayPayloadInEXT HitPayload pl;

void main() { pl.t = -1.0; }
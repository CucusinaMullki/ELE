layout(set = 0, binding = 16, rgba32f) uniform readonly image2D gbufIn;

bool loadSurface(out vec3 origin, out vec3 dir, out vec3 P, out vec3 N, out float t)
{
    ivec2 gsz = imageSize(gbufIn);
    vec2 scale = vec2(gsz) / vec2(gl_LaunchSizeEXT.xy);
    ivec2 gp = min(ivec2((vec2(gl_LaunchIDEXT.xy) + 0.5) * scale), gsz - 1);

    vec2 uv = (vec2(gp) + 0.5) / vec2(gsz);
    vec4 wp = inverse(cam.viewProjNoJitter) * vec4(uv * 2.0 - 1.0, 0.5, 1.0);
    origin = cam.camPos.xyz;
    dir = normalize(wp.xyz / wp.w - origin);

    vec4 g = imageLoad(gbufIn, gp);
    N = g.xyz;
    t = g.w;
    P = origin + dir * max(t, 0.0);
    return t > 0.0;
}

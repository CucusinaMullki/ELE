#ifndef ELE_LIGHT_GLSL
#define ELE_LIGHT_GLSL

const int LIGHT_DIRECTIONAL = 0;
const int LIGHT_POINT       = 1;
const int LIGHT_SPOT        = 2;
const int LIGHT_RECT        = 3;

struct Light
{
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 params;
    vec4 right;
};

int lightType(Light l) { return int(l.position.w + 0.5); }

float lightRangeFalloff(float dist2, float range)
{
    if (range <= 0.0) return 1.0;
    float r = dist2 / (range * range);
    float f = clamp(1.0 - r * r, 0.0, 1.0);
    return f * f;
}

bool evalLight(Light l, vec3 P, vec3 R, out vec3 L, out vec3 radiance, out float dist)
{
    const int type = lightType(l);
    const vec3 emit = l.color.rgb * l.color.a;

    if (type == LIGHT_DIRECTIONAL)
    {
        L = normalize(-l.position.xyz);
        radiance = emit;
        dist = -1.0;
        return true;
    }

    if (type == LIGHT_RECT)
    {
        const vec3 C  = l.position.xyz;
        const vec3 n  = l.direction.xyz;
        const vec3 rx = l.right.xyz;
        const vec3 ry = cross(n, rx);
        const float hw = l.params.x, hh = l.params.y;
        const bool twoSided = l.right.w > 0.5;

        float side = dot(P - C, n);
        if (side <= 0.0 && !twoSided) { L = vec3(0); radiance = vec3(0); dist = 0.0; return false; }
        const vec3 nf = side >= 0.0 ? n : -n;

        vec3 Q;
        float denom = dot(R, nf);
        if (denom < -1e-4)
        {
            float t = dot(C - P, nf) / denom;
            Q = P + R * t;
        }
        else
            Q = P - nf * dot(P - C, nf);
        vec3 lq = Q - C;
        Q = C + rx * clamp(dot(lq, rx), -hw, hw) + ry * clamp(dot(lq, ry), -hh, hh);

        vec3 toL = Q - P;
        float d2 = max(dot(toL, toL), 1e-4);
        dist = sqrt(d2);
        L = toL / dist;

        float area = 4.0 * hw * hh;
        float cosE = max(dot(-L, nf), 0.0);
        radiance = emit * (area * cosE / (d2 + area)) * lightRangeFalloff(d2, l.direction.w);
        return cosE > 0.0;
    }

    vec3 toL = l.position.xyz - P;
    float d2 = max(dot(toL, toL), 0.01);
    dist = sqrt(d2);
    L = toL / dist;
    float atten = lightRangeFalloff(d2, l.direction.w) / d2;

    if (type == LIGHT_SPOT)
    {
        float cd = dot(-L, l.direction.xyz);
        atten *= smoothstep(l.params.y, l.params.x, cd);
        if (atten <= 0.0) { radiance = vec3(0); return false; }
    }

    radiance = emit * atten;
    return true;
}

#endif

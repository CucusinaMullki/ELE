#ifndef PARTICLE_RECORD_GLSL
#define PARTICLE_RECORD_GLSL

const uint PFLAG_LOOP     = 1u;
const uint PFLAG_ADDITIVE = 2u;
const uint PFLAG_SOFT     = 4u;
const uint PFLAG_PROBES   = 8u;
const uint PFLAG_RADIAL   = 16u;
const uint PFLAG_PREWARM  = 32u;
const float TWO_PI = 6.28318530718;

const uint O_POS = 32u;
const uint O_BASIS = 35u;
const uint O_SEED = 44u;
const uint O_COUNT = 45u;
const uint O_RATE = 46u;
const uint O_FLAGS = 47u;
const uint O_SHAPE = 48u;
const uint O_SHAPESIZE = 49u;
const uint O_CONE = 52u;
const uint O_LIFE = 53u;
const uint O_SPEED = 55u;
const uint O_GRAVITY = 57u;
const uint O_DRAG = 60u;
const uint O_SIZE0 = 61u;
const uint O_SIZE1 = 63u;
const uint O_ROT0 = 65u;
const uint O_ROTSPEED = 67u;
const uint O_WOBAMP = 69u;
const uint O_WOBFREQ = 70u;
const uint O_STOPS = 71u;
const uint O_COLORS = 75u;
const uint O_FLIPC = 91u;
const uint O_FLIPR = 92u;
const uint O_FLIPFPS = 93u;
const uint O_SOFT = 94u;
const uint O_PROBEINF = 95u;
const uint O_DURATION = 96u;
const uint REC_WORDS = 97u;

layout(std430, binding = EFFECT_BINDING) readonly buffer EffectBuffer { uint effectWords[]; };

float rf(uint base, uint w)  { return uintBitsToFloat(effectWords[base + w]); }
uint  ru(uint base, uint w)  { return effectWords[base + w]; }
vec2  rv2(uint base, uint w) { return vec2(rf(base, w), rf(base, w + 1u)); }
vec3  rv3(uint base, uint w) { return vec3(rf(base, w), rf(base, w + 1u), rf(base, w + 2u)); }
vec4  rv4(uint base, uint w) { return vec4(rf(base, w), rf(base, w + 1u), rf(base, w + 2u), rf(base, w + 3u)); }

struct ParticleInstance
{
    vec4 posSize;
    vec4 color;
    vec4 misc;
};

#endif

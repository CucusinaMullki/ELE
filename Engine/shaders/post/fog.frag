#version 450

layout(binding = 0) uniform sampler2D depthTexture;

layout(binding = 1) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    mat4 lightSpaceMatrix;
    vec4 camPos;
} cam;

layout(binding = 2) uniform sampler2DShadow shadowMap;

#include "light.glsl"
layout(binding = 3) uniform LightUBO {
    Light lights[16];
    ivec4 count;
} lightData;

layout(push_constant) uniform Push {
    vec4 scatteringColorDensity;
    vec4 heightParams;
    vec4 lightParams;
    vec4 miscParams;
} fog;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float sampleSunShadow(vec3 worldPos)
{
    vec4 lsPos = cam.lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z < 0.0 || proj.z > 1.0)
        return 1.0;

    return texture(shadowMap, vec3(proj.xy, proj.z - 0.001));
}

float henyeyGreenstein(float cosAngle, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5));
}

void main()
{
    vec3 scatteringColor = fog.scatteringColorDensity.rgb;
    float density        = fog.scatteringColorDensity.a;
    float heightFalloff   = fog.heightParams.x;
    float heightStart     = fog.heightParams.y;
    float maxDistance     = fog.heightParams.z;
    int   stepCount       = int(fog.heightParams.w);
    float ambientIntensity = fog.lightParams.x;
    float sunIntensity     = fog.lightParams.y;
    float noiseScale       = fog.lightParams.z;
    float noiseSpeed       = fog.lightParams.w;
    float time              = fog.miscParams.x;

    float sceneDepth = texture(depthTexture, fragUV).r;

    if (sceneDepth >= 0.9999)
    {
        outColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    vec4 ndc = vec4(fragUV * 2.0 - 1.0, sceneDepth, 1.0);
    vec4 worldPosH = cam.invViewProj * ndc;
    vec3 scenePos = worldPosH.xyz / worldPosH.w;

    vec3 camPos = cam.camPos.xyz;
    vec3 toScene = scenePos - camPos;
    float sceneDist = length(toScene);
    vec3 rayDir = toScene / max(sceneDist, 0.0001);

    float rayLength = min(sceneDist, maxDistance);
    float dt = rayLength / float(stepCount);

    vec3 sunColor = vec3(0.0);
    float sunPower = 0.0;
    if (lightData.count.x > 0 && lightData.lights[0].position.w < 0.5)
    {
        sunColor = lightData.lights[0].color.rgb;
        sunPower = lightData.lights[0].color.a;
    }

    vec3 pos = camPos;
    float transmittance = 1.0;
    vec3 inscatter = vec3(0.0);

    for (int i = 0; i < stepCount; i++)
    {
        pos += rayDir * dt;

        float heightAtten = exp(-max(pos.y - heightStart, 0.0) * heightFalloff);
        float n = hash(pos * noiseScale + vec3(time * noiseSpeed, 0.0, 0.0));
        float localDensity = density * heightAtten * mix(0.6, 1.4, n);

        float stepTransmittance = exp(-localDensity * dt);

        float visibility = sampleSunShadow(pos);
        vec3 sunDir = vec3(0.0, 1.0, 0.0);
        if (lightData.count.x > 0 && lightData.lights[0].position.w < 0.5)
            sunDir = normalize(-lightData.lights[0].position.xyz);

        float cosAngle = dot(rayDir, sunDir);
        float phase = henyeyGreenstein(cosAngle, 0.6);
        vec3 sunContribution = sunColor * sunPower * visibility * sunIntensity * phase;
        vec3 ambientContribution = scatteringColor * ambientIntensity;

        vec3 stepInscatter = (sunContribution + ambientContribution) * scatteringColor * localDensity * dt;

        inscatter += stepInscatter * transmittance;
        transmittance *= stepTransmittance;
    }

    float dither = (hash(vec3(gl_FragCoord.xy, 0.0)) - 0.5) / 255.0;
    outColor = vec4(inscatter + dither, 1.0 - transmittance);
}

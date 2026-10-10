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

layout(location = 0) in vec4  vColor;
layout(location = 1) in vec2  vUV;
layout(location = 2) in float vViewDepth;
layout(location = 3) flat in float vSoft;

layout(location = 0) out vec4 outColor;

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
    outColor = vec4(tex.rgb * vColor.rgb, a);
}

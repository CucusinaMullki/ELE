#version 450

layout(binding = 0) uniform sampler2D decalTexture;
layout(binding = 1) uniform sampler2D depthTexture;

layout(binding = 2) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
} cam;

layout(push_constant) uniform Push {
    mat4 model;
    mat4 invModel;
} push;

layout(location = 0) in vec4 fragClipPos;

layout(location = 0) out vec4 outColor;

void main()
{
    // 1-2. screenUV фрагмента бокса + глубина сцены в этой точке
    vec2 screenUV = (fragClipPos.xy / fragClipPos.w) * 0.5 + 0.5;
    float sceneDepth = texture(depthTexture, screenUV).r;

    // 3. восстанавливаем world position ИЗ ГЛУБИНЫ СЦЕНЫ, а не из глубины самого бокса
    vec4 ndc = vec4(screenUV * 2.0 - 1.0, sceneDepth, 1.0);
    vec4 worldPosH = cam.invViewProj * ndc;
    vec3 worldPos = worldPosH.xyz / worldPosH.w;

    // 4. в локальные координаты декали
    vec3 localPos = (push.invModel * vec4(worldPos, 1.0)).xyz;

    // 5. отбрасываем всё, что вне unit-куба [-0.5, 0.5]
    if (any(greaterThan(abs(localPos), vec3(0.5))))
        discard;

    // проекция "сверху вниз" вдоль локальной Y: XZ -> UV
    vec2 decalUV = localPos.xz + 0.5;
    vec4 texColor = texture(decalTexture, decalUV);

    if (texColor.a < 0.01)
        discard;

    outColor = texColor;
}
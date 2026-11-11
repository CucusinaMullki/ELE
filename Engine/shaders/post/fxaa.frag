#version 450

layout(binding = 0) uniform sampler2D srcTexture;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL 1.0/8.0
#define FXAA_REDUCE_MIN 1.0/128.0
#define FXAA_SUBPIX_QUALITY 0.75

float luma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(srcTexture, 0));

    vec3 rgbNW = texture(srcTexture, fragUV + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 rgbNE = texture(srcTexture, fragUV + texelSize * vec2( 1.0, -1.0)).rgb;
    vec3 rgbSW = texture(srcTexture, fragUV + texelSize * vec2(-1.0,  1.0)).rgb;
    vec3 rgbSE = texture(srcTexture, fragUV + texelSize * vec2( 1.0,  1.0)).rgb;
    vec3 rgbM  = texture(srcTexture, fragUV).rgb;

    float lumaNW = luma(rgbNW);
    float lumaNE = luma(rgbNE);
    float lumaSW = luma(rgbSW);
    float lumaSE = luma(rgbSE);
    float lumaM  = luma(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir * rcpDirMin)) * texelSize;

    vec3 rgbA = 0.5 * (
        texture(srcTexture, fragUV + dir * (1.0/3.0 - 0.5)).rgb +
        texture(srcTexture, fragUV + dir * (2.0/3.0 - 0.5)).rgb
    );
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(srcTexture, fragUV + dir * -0.5).rgb +
        texture(srcTexture, fragUV + dir *  0.5).rgb
    );

    float lumaB = luma(rgbB);

    if (lumaB < lumaMin || lumaB > lumaMax) {
        outColor = vec4(rgbA, 1.0);
    } else {
        outColor = vec4(rgbB, 1.0);
    }

    float lumaAverage = (1.0/12.0) * (2.0 * (lumaNW + lumaNE + lumaSW + lumaSE) + lumaM);
    float subpix1 = lumaAverage - lumaM;
    float subpix2 = subpix1 * subpix1;
    float subpix3 = subpix2 * subpix2;
    float subpix = clamp((-subpix3) * 8.0 + FXAA_SUBPIX_QUALITY, 0.0, 1.0);

    vec3 finalColor = mix(outColor.rgb, rgbM, subpix);
    outColor = vec4(finalColor, 1.0);
}

#version 450

layout(binding = 0) uniform sampler2D image;

layout(push_constant) uniform Push 
{
    vec2 direction;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(image, 0));
    vec3 result = vec3(0.0);

    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    result += texture(image, fragUV).rgb * weights[0];

    for (int i = 1; i < 5; i++)
    {
        vec2 offset = push.direction * texelSize * float(i);
        result += texture(image, fragUV + offset).rgb * weights[i];
        result += texture(image, fragUV - offset).rgb * weights[i];
    }

    outColor = vec4(result, 1.0);    
}
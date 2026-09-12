#version 450

layout(location = 0) in vec3 fragLocalPos;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform samplerCube environmentMap;

void main()
{
    vec3 color = texture(environmentMap, fragLocalPos).rgb;
    color = color / (color + vec3(1.0));
    outColor = vec4(color, 1.0);
}

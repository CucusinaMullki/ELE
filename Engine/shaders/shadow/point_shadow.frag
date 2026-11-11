#version 450

layout(push_constant) uniform Push
{
    mat4 lightViewProj;
    mat4 model;
    vec4 lightPos;
} push;

layout(location = 0) in vec3 fragWorldPos;

void main()
{
    float dist = length(fragWorldPos - push.lightPos.xyz);
    gl_FragDepth = dist / push.lightPos.w;
}

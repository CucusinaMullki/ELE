#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform Push
{
    mat4 viewProj;
} push;

layout(location = 0) out vec3 fragLocalPos;

void main()
{
    fragLocalPos = inPosition;
    gl_Position = push.viewProj * vec4(inPosition, 1.0);
}

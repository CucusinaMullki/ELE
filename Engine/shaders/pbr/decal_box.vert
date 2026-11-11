#version 450

layout(binding = 2) uniform Camera {
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
} cam;

layout(push_constant) uniform Push {
    mat4 model;
    mat4 invModel;
} push;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragClipPos;

void main()
{
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = cam.proj * cam.view * worldPos;
    fragClipPos = gl_Position;
}

#version 450

layout(push_constant) uniform Push {
    mat4 lightSpaceMatrix;
    mat4 model;
} push;

layout(location = 0) in vec3 inPosition;
out gl_PerVertex { vec4 gl_Position; };

void main() {
    vec4 pos = push.lightSpaceMatrix * push.model * vec4(inPosition, 1.0);
    gl_Position = pos;
}
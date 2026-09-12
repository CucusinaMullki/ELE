#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform Camera
{
    mat4 view;
    mat4 proj;
} cam;

layout(location = 0) out vec3 fragLocalPos;

void main()
{
    fragLocalPos = inPosition;

    mat4 viewNoTranslation = mat4(mat3(cam.view));
    vec4 pos = cam.proj * viewNoTranslation * vec4(inPosition, 1.0);

    gl_Position = pos.xyww;
}

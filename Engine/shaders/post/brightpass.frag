#version 450

layout(binding = 0) uniform sampler2D hdrBuffer;

layout(push_constant) uniform Push
{
    float threshold;
} push;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = texture(hdrBuffer, fragUV).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    float knee = push.threshold * 0.5 + 1e-4;
    float soft = clamp(brightness - push.threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);

    float contribution = max(soft, brightness - push.threshold);
    contribution /= max(brightness, 1e-4);

    outColor = vec4(color * contribution, 1.0);
}

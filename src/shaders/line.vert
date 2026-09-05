#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(binding = 0) uniform UBO {
    mat4 viewProj;
    vec4 camPos;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = inColor;
    gl_Position = ubo.viewProj * pc.model * vec4(inPosition, 1.0);
}

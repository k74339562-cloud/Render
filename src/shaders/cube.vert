#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(binding = 0) uniform UBO {
    mat4 viewProj;
    mat4 model;
    vec4 camPos;
} ubo;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(ubo.model) * inNormal;
    gl_Position = ubo.viewProj * worldPos;
}

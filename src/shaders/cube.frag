#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;

layout(binding = 0) uniform UBO {
    mat4 viewProj;
    mat4 model;
    vec4 camPos;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(fragNormal);
    vec3 v = normalize(ubo.camPos.xyz - fragWorldPos);

    // إضاءة استوديو بلندر الرباعية بنظام Z-Up
    vec3 keyDir = normalize(vec3(0.5, -0.6, 0.8));
    vec3 keyCol = vec3(1.0, 0.96, 0.90);
    float diffKey = max(dot(n, keyDir), 0.0);

    vec3 fillDir = normalize(vec3(-0.6, -0.5, 0.3));
    vec3 fillCol = vec3(0.65, 0.78, 0.95);
    float diffFill = max(dot(n, fillDir), 0.0) * 0.45;

    vec3 backDir = normalize(vec3(-0.2, 0.8, 0.6));
    vec3 backCol = vec3(0.85, 0.90, 1.0);
    float diffBack = max(dot(n, backDir), 0.0) * 0.30;

    vec3 bounceDir = vec3(0.0, 0.0, -1.0);
    vec3 bounceCol = vec3(0.35, 0.35, 0.38);
    float diffBounce = max(dot(n, bounceDir), 0.0) * 0.20;

    // بريق بلندر البصري (Specular Blinn-Phong)
    vec3 halfKey = normalize(keyDir + v);
    float spec = pow(max(dot(n, halfKey), 0.0), 28.0) * 0.35;

    // وهج الحواف الأبيض الناعم (Fresnel Rim Glow)
    float NdotV = max(dot(n, v), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.0) * 0.40;
    vec3 rimCol = vec3(0.9, 0.95, 1.0) * fresnel;

    // خامة طين بلندر الأصلية (Solid Clay)
    vec3 baseClay = vec3(0.58, 0.58, 0.62);
    vec3 totalLight = (keyCol * diffKey) + (fillCol * diffFill) + (backCol * diffBack) + (bounceCol * diffBounce) + vec3(0.16);
    vec3 linearColor = (baseClay * totalLight) + vec3(spec) + rimCol;

    // تصحيح الألوان (sRGB Gamma 2.2) لإنهاء بهتان الألوان وجعلها عميقة كبلندر
    vec3 srgbColor = pow(linearColor, vec3(1.0 / 2.2));
    outColor = vec4(srgbColor, 1.0);
}

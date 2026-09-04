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

    // 1. الضوء الرئيسي (Key Light)
    vec3 keyDir = normalize(vec3(0.6, 0.85, 0.55));
    vec3 keyCol = vec3(1.0, 0.96, 0.90);
    float diffKey = max(dot(n, keyDir), 0.0);

    // 2. ضوء التعبئة الناعم (Fill Light)
    vec3 fillDir = normalize(vec3(-0.65, 0.3, 0.5));
    vec3 fillCol = vec3(0.70, 0.82, 1.0);
    float diffFill = max(dot(n, fillDir), 0.0) * 0.45;

    // 3. الضوء الخلفي الحاد (Rim/Back Light)
    vec3 backDir = normalize(vec3(0.0, -0.6, -0.8));
    vec3 backCol = vec3(0.8, 0.85, 0.95);
    float diffBack = max(dot(n, backDir), 0.0) * 0.25;

    // 4. الضوء الارتدادي من الأرضية (Ground Bounce)
    vec3 bounceDir = normalize(vec3(0.0, -1.0, 0.0));
    vec3 bounceCol = vec3(0.4, 0.45, 0.5);
    float diffBounce = max(dot(n, bounceDir), 0.0) * 0.2;

    // لمعان بريق السطح (Specular Blinn-Phong)
    vec3 halfKey = normalize(keyDir + v);
    float spec = pow(max(dot(n, halfKey), 0.0), 32.0) * 0.35;

    // وهج الحواف الأبيض الشهير في بلندر (Fresnel Rim Glow)
    float NdotV = max(dot(n, v), 0.0);
    float fresnel = pow(1.0 - NdotV, 2.8) * 0.35;
    vec3 rimCol = vec3(0.9, 0.95, 1.0) * fresnel;

    // لون خامة طين بلندر الأصلي (Studio Clay)
    vec3 baseClay = vec3(0.56, 0.58, 0.62);

    vec3 totalLighting = (keyCol * diffKey) + (fillCol * diffFill) + (backCol * diffBack) + (bounceCol * diffBounce) + vec3(0.18);
    vec3 finalColor = (baseClay * totalLighting) + vec3(spec) + rimCol;

    outColor = vec4(finalColor, 1.0);
}

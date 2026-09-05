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
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.camPos.xyz - fragWorldPos);

    // حساب متجهات فضاء الكاميرا
    vec3 camFwd = -V;
    vec3 camRight = normalize(cross(vec3(0.0, 0.0, 1.0), camFwd));
    if (length(camRight) < 0.001) camRight = vec3(1.0, 0.0, 0.0);
    vec3 camUp = cross(camFwd, camRight);

    // 1. الضوء الرئيسي (Key Light) - من أعلى يمين الكاميرا
    vec3 keyDir = normalize(camRight * 0.45 + camUp * 0.75 - camFwd * 0.45);
    vec3 keyCol = vec3(1.0, 0.98, 0.95);
    float diffKey = max(dot(N, keyDir), 0.0);

    // 2. ضوء التعبئة (Fill Light) - من يسار الكاميرا
    vec3 fillDir = normalize(-camRight * 0.65 + camUp * 0.20 - camFwd * 0.35);
    vec3 fillCol = vec3(0.55, 0.68, 0.85);
    float diffFill = max(dot(N, fillDir), 0.0) * 0.45;

    // 3. الضوء الخلفي لإبراز زوايا الحواف (Rim Light)
    vec3 rimDir = normalize(-camUp * 0.70 + camFwd * 0.65);
    vec3 rimCol = vec3(0.85, 0.92, 1.00);
    float diffRim = max(dot(N, rimDir), 0.0) * 0.35;

    // 4. ارتداد الإضاءة من الأرضية
    vec3 bounceCol = vec3(0.35, 0.35, 0.37);
    float diffBounce = max(dot(N, vec3(0.0, 0.0, -1.0)), 0.0) * 0.20;

    // بريق بلندر Blinn-Phong السطحي
    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.25;

    // وهج بلندر الجانبي (Fresnel Rim)
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.5) * 0.35;
    vec3 fresnelCol = vec3(0.95, 0.98, 1.0) * fresnel;

    // خامة طين بلندر الأصلية (Solid Clay Workbench)
    vec3 baseClay = vec3(0.68, 0.68, 0.71);
    vec3 ambient = vec3(0.20, 0.20, 0.22);

    vec3 totalLight = ambient + (keyCol * diffKey) + (fillCol * diffFill) + (rimCol * diffRim) + (bounceCol * diffBounce);
    vec3 linearColor = (baseClay * totalLight) + vec3(spec) + fresnelCol;

    // تصحيح الألوان sRGB Gamma 2.2
    outColor = vec4(pow(linearColor, vec3(1.0 / 2.2)), 1.0);
}

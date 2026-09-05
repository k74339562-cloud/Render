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

    // بناء محاور الكاميرا في الفضاء العالمي لجعل الإضاءة تدور مع عين المشاهد كبلندر
    vec3 camFwd = -V;
    vec3 camRight = normalize(cross(vec3(0.0, 0.0, 1.0), camFwd));
    if (length(camRight) < 0.001) camRight = vec3(1.0, 0.0, 0.0);
    vec3 camUp = cross(camFwd, camRight);

    // 1. الضوء الرئيسي (Key Light) - قادم من يمين وأعلى الكاميرا
    vec3 keyDir = normalize(camRight * 0.45 + camUp * 0.65 - camFwd * 0.60);
    vec3 keyCol = vec3(1.0, 0.98, 0.93);
    float diffKey = max(dot(N, keyDir), 0.0);

    // 2. ضوء التعبئة (Fill Light) - قادم من يسار الكاميرا بلمسة باردة
    vec3 fillDir = normalize(-camRight * 0.60 + camUp * 0.20 - camFwd * 0.50);
    vec3 fillCol = vec3(0.65, 0.75, 0.90);
    float diffFill = max(dot(N, fillDir), 0.0) * 0.45;

    // 3. الضوء الخلفي (Rim Light)
    vec3 rimDir = normalize(-camUp * 0.60 + camFwd * 0.70);
    vec3 rimCol = vec3(0.85, 0.90, 1.00);
    float diffRim = max(dot(N, rimDir), 0.0) * 0.30;

    // 4. الضوء المرتد من الأرضية (Ground Bounce)
    vec3 bounceCol = vec3(0.35, 0.35, 0.38);
    float diffBounce = max(dot(N, vec3(0.0, 0.0, -1.0)), 0.0) * 0.25;

    // بريق بلندر Blinn-Phong Specular
    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.28;

    // وهج الحواف المميز لبلندر (Fresnel Rim Glow)
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.5) * 0.35;
    vec3 fresnelCol = vec3(0.92, 0.95, 1.0) * fresnel;

    // لون طين بلندر الأصلي (Blender Solid Clay)
    vec3 baseClay = vec3(0.64, 0.64, 0.67);
    vec3 ambient = vec3(0.17, 0.17, 0.19);

    vec3 totalLight = ambient + (keyCol * diffKey) + (fillCol * diffFill) + (rimCol * diffRim) + (bounceCol * diffBounce);
    vec3 linearColor = (baseClay * totalLight) + vec3(spec) + fresnelCol;

    // تصحيح الألوان sRGB Gamma 2.2
    outColor = vec4(pow(linearColor, vec3(1.0 / 2.2)), 1.0);
}

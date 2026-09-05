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

    // بناء فضاء الكاميرا في العالم الحقيقي
    vec3 camFwd = -V;
    vec3 camRight = normalize(cross(vec3(0.0, 0.0, 1.0), camFwd));
    if (length(camRight) < 0.001) camRight = vec3(1.0, 0.0, 0.0);
    vec3 camUp = cross(camFwd, camRight);

    // 1. الضوء الرئيسي (Key Light) - من أعلى اليمين
    vec3 keyDir = normalize(camRight * 0.50 + camUp * 0.65 - camFwd * 0.55);
    vec3 keyCol = vec3(1.0, 0.98, 0.94);
    float diffKey = max(dot(N, keyDir), 0.0);

    // 2. ضوء التعبئة (Fill Light) - إضاءة باردة من اليسار
    vec3 fillDir = normalize(-camRight * 0.60 + camUp * 0.30 - camFwd * 0.45);
    vec3 fillCol = vec3(0.62, 0.74, 0.92);
    float diffFill = max(dot(N, fillDir), 0.0) * 0.48;

    // 3. الضوء الخلفي لإبراز زوايا وحواف المجسم (Rim Light)
    vec3 rimDir = normalize(-camUp * 0.70 + camFwd * 0.65);
    vec3 rimCol = vec3(0.88, 0.93, 1.00);
    float diffRim = max(dot(N, rimDir), 0.0) * 0.35;

    // 4. الضوء المرتد من الأرضية (Ground Bounce)
    vec3 bounceCol = vec3(0.36, 0.36, 0.38);
    float diffBounce = max(dot(N, vec3(0.0, 0.0, -1.0)), 0.0) * 0.25;

    // بريق بلندر Blinn-Phong السطحي المات المطفأ
    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), 38.0) * 0.28;

    // وهج بلندر الجانبي (Fresnel Silhouette Glow)
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.2) * 0.35;
    vec3 fresnelCol = vec3(0.95, 0.98, 1.0) * fresnel;

    // ميزة Cavity / Ridge: إضاءة الحواف البارزة كبلندر الحقيقي
    vec3 dNdx = dFdx(N);
    vec3 dNdy = dFdy(N);
    float edgeCavity = length(dNdx) + length(dNdy);
    float ridge = smoothstep(0.02, 0.25, edgeCavity) * 0.25;

    // خامة طين بلندر الأصلية (Blender Workbench Solid Clay)
    vec3 baseClay = vec3(0.67, 0.67, 0.70);
    vec3 ambient = vec3(0.22, 0.22, 0.24);

    vec3 totalLight = ambient + (keyCol * diffKey) + (fillCol * diffFill) + (rimCol * diffRim) + (bounceCol * diffBounce);
    vec3 linearColor = (baseClay * totalLight) + vec3(spec) + fresnelCol + vec3(ridge);

    // تصحيح الألوان الجاما sRGB 2.2
    outColor = vec4(pow(linearColor, vec3(1.0 / 2.2)), 1.0);
}

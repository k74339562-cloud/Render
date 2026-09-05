#include "camera.h"
#include <cmath>
#include <algorithm>

void Camera::onOrbit(float dx, float dy) {
    // تم ضبط الإشارات بدقة متناهية لتتبع حركة إصبعك 1:1
    // السحب لليمين يحرك المجسم لليمين، والسحب لأعلى يحرك المجسم لأعلى
    yaw -= dx * 0.005f;
    pitch = std::clamp(pitch + dy * 0.005f, -1.52f, 1.52f);
}

void Camera::onZoom(float ratio) {
    if (ratio <= 0.001f) return;
    // زوم مقيد بحدود صارمة يمنع الهروب إلى الفراغ الأسود نهائياً
    dist = std::clamp(dist / ratio, 1.8f, 45.0f);
}

void Camera::onPan(float dx, float dy) {
    float cosY = std::cos(yaw), sinY = std::sin(yaw);
    Vec3 camRight = {-cosY, sinY, 0.0f};
    Vec3 camUp = {-sinY * std::sin(pitch), -cosY * std::sin(pitch), std::cos(pitch)};

    // تحريك متوازن يتبع حركة الإصبعين بدقة
    float factor = dist * 0.001f;
    Vec3 deltaMove = (camRight * (-dx * factor)) + (camUp * (-dy * factor));
    target = target + deltaMove;
}

void Camera::focusOn(const Vec3& point) {
    // إعادة ضبط الكاميرا على المكعب فوراً (مثل زر النقطة في بلندر)
    target = point;
}

Vec3 Camera::getPosition() const {
    return target + Vec3(
        dist * std::cos(pitch) * std::sin(yaw),
        dist * std::cos(pitch) * std::cos(yaw),
        dist * std::sin(pitch)
    );
}

Mat4 Camera::getViewMatrix() const {
    Vec3 eye = getPosition();
    return Mat4::lookAt(eye, target, Vec3(0.0f, 0.0f, 1.0f));
}

Mat4 Camera::getProjectionMatrix(float width, float height) const {
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;
    float fovRad = 45.0f * (3.14159265f / 180.0f);
    float nearZ = 0.1f, farZ = 200.0f;

    Mat4 r;
    float tanHalf = std::tan(fovRad * 0.5f);
    r.m[0] = 1.0f / (aspect * tanHalf);
    r.m[5] = 1.0f / tanHalf;
    r.m[10] = farZ / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = -(farZ * nearZ) / (farZ - nearZ);
    return r;
}

Vec2 Camera::projectToScreen(const Vec3& worldPos, float screenW, float screenH) const {
    Mat4 vp = getProjectionMatrix(screenW, screenH) * getViewMatrix();
    Vec3 ndc = vp.transformPoint(worldPos);

    float px = (ndc.x + 1.0f) * 0.5f * screenW;
    float py = (1.0f - ndc.y) * 0.5f * screenH;
    return {px, py};
}

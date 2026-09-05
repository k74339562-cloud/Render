#include "camera.h"
#include <cmath>
#include <algorithm>

void Camera::onOrbit(float dx, float dy) {
    // تم عكس الإشارات: السحب لليمين يلف المشهد لليمين، والسحب لأعلى يرفع الرؤية لأعلى
    yaw += dx * 0.005f;
    pitch = std::clamp(pitch - dy * 0.005f, -1.55f, 1.55f);
}

void Camera::onZoom(float delta) {
    dist = std::clamp(dist * (1.0f - delta * 0.004f), 1.2f, 80.0f);
}

void Camera::onPan(float dx, float dy) {
    // حركة متوازية ودقيقة مع شاشة العرض بنظام Z-Up
    float cosY = std::cos(yaw), sinY = std::sin(yaw);
    Vec3 right = {-cosY, sinY, 0.0f};
    Vec3 up = {-sinY * std::sin(pitch), -cosY * std::sin(pitch), std::cos(pitch)};

    float factor = dist * 0.001f;
    ofs = ofs - (right * (dx * factor)) + (up * (dy * factor));
}

Vec3 Camera::getPosition() const {
    return ofs + Vec3(
        dist * std::cos(pitch) * std::sin(yaw),
        dist * std::cos(pitch) * std::cos(yaw),
        dist * std::sin(pitch)
    );
}

Mat4 Camera::getViewMatrix() const {
    Vec3 eye = getPosition();
    return Mat4::lookAt(eye, ofs, Vec3(0.0f, 0.0f, 1.0f)); // +Z هو الأعلى دائماً كبلندر
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

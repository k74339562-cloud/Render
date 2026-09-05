#include "camera.h"
#include <cmath>
#include <algorithm>

Vec3 Camera::getForward() const {
    // متجه النظر الحر بنظام Z-Up
    return Vec3(
        std::cos(pitch) * std::sin(yaw),
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch)
    ).normalize();
}

Vec3 Camera::getRight() const {
    float cosY = std::cos(yaw), sinY = std::sin(yaw);
    return Vec3(-cosY, sinY, 0.0f).normalize();
}

Vec3 Camera::getUp() const {
    return getRight().cross(getForward()).normalize();
}

void Camera::onLook(float dx, float dy) {
    // دوران حر تماماً في مكان الكاميرا دون أي نقطة ارتكاز
    yaw += dx * 0.004f;
    pitch = std::clamp(pitch + dy * 0.004f, -1.52f, 1.52f);
}

void Camera::onPan(float dx, float dy) {
    // تحريك حر في مستوي الرؤية
    Vec3 right = getRight();
    Vec3 up = getUp();
    float speed = 0.008f;
    pos = pos + (right * (-dx * speed)) + (up * (dy * speed));
}

void Camera::onFly(float delta) {
    // طيران للأمام والخلف بنعومة
    Vec3 fwd = getForward();
    float speed = 0.025f;
    pos = pos + (fwd * (delta * speed));
}

Mat4 Camera::getViewMatrix() const {
    return Mat4::lookAt(pos, pos + getForward(), Vec3(0.0f, 0.0f, 1.0f));
}

Mat4 Camera::getProjectionMatrix(float width, float height) const {
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;
    float fovRad = 50.0f * (3.14159265f / 180.0f);
    float nearZ = 0.1f, farZ = 300.0f;

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

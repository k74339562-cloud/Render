#include "camera.h"
#include <algorithm>

void Camera::onRotate(float dx, float dy) {
    yaw -= dx * 0.007f;
    pitch = std::clamp(pitch + dy * 0.007f, -1.45f, 1.45f);
}

void Camera::onPan(float dx, float dy) {
    Vec3 f = (target - getPosition()).normalize();
    Vec3 r = f.cross(Vec3(0, 1, 0)).normalize();
    Vec3 u = r.cross(f);

    float factor = distance * 0.0015f;
    target = target - (r * dx * factor) + (u * dy * factor);
}

void Camera::onZoom(float delta) {
    distance = std::clamp(distance - delta * 0.015f, 1.8f, 40.0f);
}

Vec3 Camera::getPosition() const {
    return target + Vec3(
        distance * std::cos(pitch) * std::sin(yaw),
        distance * std::sin(pitch),
        distance * std::cos(pitch) * std::cos(yaw)
    );
}

Mat4 Camera::getViewMatrix() const {
    return Mat4::lookAt(getPosition(), target, Vec3(0, 1, 0));
}

Mat4 Camera::getProjectionMatrix(float width, float height) const {
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;
    return Mat4::perspective(45.0f * (3.14159265f / 180.0f), aspect, 0.1f, 150.0f);
}

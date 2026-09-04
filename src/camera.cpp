#include "camera.h"
#include <algorithm>

void Camera::onRotate(float dx, float dy) {
    yaw -= dx * 0.008f;
    pitch = std::clamp(pitch + dy * 0.008f, -1.45f, 1.45f);
}

void Camera::onZoom(float delta) {
    distance = std::clamp(distance - delta * 0.5f, 2.0f, 35.0f);
}

Vec3 Camera::getPosition() const {
    return {
        distance * std::cos(pitch) * std::sin(yaw),
        distance * std::sin(pitch),
        distance * std::cos(pitch) * std::cos(yaw)
    };
}

Mat4 Camera::getViewMatrix() const {
    return Mat4::lookAt(getPosition(), Vec3(0, 0, 0), Vec3(0, 1, 0));
}

Mat4 Camera::getProjectionMatrix(float width, float height) const {
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;
    return Mat4::perspective(45.0f * (3.14159265f / 180.0f), aspect, 0.1f, 100.0f);
}

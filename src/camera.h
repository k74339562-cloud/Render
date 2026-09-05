#pragma once
#include "math_3d.h"

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

class Camera {
public:
    Vec3 pos = {5.0f, 5.0f, 4.0f}; // موقع الكاميرا الحر في العالم
    float yaw = -2.356f;            // النظر نحو نقطة الأصل
    float pitch = -0.45f;           // زاوية النظر لأسفل قليلاً

    void onLook(float dx, float dy);
    void onPan(float dx, float dy);
    void onFly(float delta);

    Vec3 getForward() const;
    Vec3 getRight() const;
    Vec3 getUp() const;

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;

    Vec2 projectToScreen(const Vec3& worldPos, float screenW, float screenH) const;
};

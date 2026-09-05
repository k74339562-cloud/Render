#pragma once
#include "math_3d.h"

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

class Camera {
public:
    Vec3 target = {0.0f, 0.0f, 0.0f}; // مركز الارتكاز المباشر
    float dist = 7.0f;                 // مسافة الكاميرا عن المركز
    float yaw = -0.785f;               // زاوية بلندر الكلاسيكية
    float pitch = 0.523f;              // زاوية الارتفاع

    void onOrbit(float dx, float dy);
    void onZoom(float ratio);
    void onPan(float dx, float dy);
    void focusOn(const Vec3& point);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;

    Vec2 projectToScreen(const Vec3& worldPos, float screenW, float screenH) const;
};

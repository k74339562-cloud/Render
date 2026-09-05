#pragma once
#include "math_3d.h"

class Camera {
public:
    Vec3 ofs = {0.0f, 0.0f, 0.0f}; // نقطة ارتكاز المكعب (Target)
    float dist = 6.0f;              // المسافة (Distance)
    float yaw = 0.785f;             // زاوية الدوران الأفقية
    float pitch = 0.523f;           // زاوية الدوران الرأسية

    void onOrbit(float dx, float dy);
    void onZoom(float delta);
    void onPan(float dx, float dy);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;
};

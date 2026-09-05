#pragma once
#include "math_3d.h"

class Camera {
public:
    Vec3 ofs = {0.0f, 0.0f, 0.0f}; // نقطة ارتكاز الكاميرا (Pivot)
    float dist = 7.0f;              // المسافة عن الهدف
    float yaw = -0.785f;            // الدوران الأفقي (زاوية بلندر الكلاسيكية)
    float pitch = 0.523f;           // زاوية الارتفاع

    void onOrbit(float dx, float dy);
    void onZoom(float delta);
    void onPan(float dx, float dy);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;
};

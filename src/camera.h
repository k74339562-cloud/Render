#pragma once
#include "math_3d.h"

class Camera {
public:
    // متغيرات RegionView3D الرسمية في بلندر
    Vec3 ofs = {0.0f, 0.0f, 0.0f}; // مركز المكعب (مقفول تماماً أثناء الدوران)
    float dist = 6.0f;              // المسافة بين الكاميرا والمركز
    float yaw = 0.785f;             // زاوية الدوران الأفقية
    float pitch = 0.523f;           // زاوية الدوران الرأسية

    void onOrbit(float dx, float dy);
    void onZoom(float delta);
    void onPan(float dx, float dy);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;
};

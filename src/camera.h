#pragma once
#include "math_3d.h"

class Camera {
public:
    Vec3 ofs = {0.0f, 0.0f, 0.0f};
    float dist = 7.0f;
    float yaw = -0.785f;
    float pitch = 0.523f;

    void onOrbit(float dx, float dy);
    void onZoom(float delta);
    void onPan(float dx, float dy);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;

    // دالة إطلاق الشعاع من شاشة اللمس إلى الفضاء 3D
    Ray getScreenRay(float touchX, float touchY, float screenW, float screenH) const;
};

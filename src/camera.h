#pragma once
#include "math_3d.h"

class Camera {
public:
    Vec3 ofs = {0.0f, 0.0f, 0.0f};
    float dist = 7.2f;
    float yaw = -0.785f;  // زاوية بلندر الكلاسيكية (-45 درجة)
    float pitch = 0.523f; // زاوية بلندر الكلاسيكية (30 درجة)

    void onOrbit(float dx, float dy);
    void onZoom(float delta);
    void onPan(float dx, float dy);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;
};

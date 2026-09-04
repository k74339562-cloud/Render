#pragma once
#include "math_3d.h"

class Camera {
public:
    float yaw = 0.785f;
    float pitch = 0.52f;
    float distance = 7.0f;
    Vec3 target = {0.0f, 0.0f, 0.0f};

    void onRotate(float dx, float dy);
    void onPan(float dx, float dy);
    void onZoom(float delta);
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;
};

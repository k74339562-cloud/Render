#pragma once
#include "math_3d.h"

class Camera {
public:
    float yaw = 0.75f;
    float pitch = 0.55f;
    float distance = 7.0f;

    void onRotate(float dx, float dy);
    void onZoom(float delta);
    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(float width, float height) const;
    Vec3 getPosition() const;
};

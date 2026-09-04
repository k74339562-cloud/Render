#pragma once
#include "math_3d.h"
#include <GLES3/gl3.h>

class Gizmo {
public:
    GLuint vbo = 0;
    int activeAxis = -1; // -1: لا شيء، 0: X، 1: Y، 2: Z

    void init();
    void render(const Mat4& vp);
    void checkHit(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& cubePos);
};

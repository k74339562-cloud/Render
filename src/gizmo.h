#pragma once
#include "math_3d.h"
#include <vector>

struct GizmoVertex {
    float x, y, z;
    float r, g, b, a;
};

class Gizmo {
public:
    std::vector<GizmoVertex> vertices;
    void init();
};

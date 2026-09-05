#include "gizmo.h"
#include <cmath>

static void addCone(std::vector<GizmoVertex>& v, const Vec3& base, const Vec3& dir, float r, float g, float b) {
    float len = 0.45f, radius = 0.12f;
    Vec3 tip = base + dir * len;
    Vec3 p1 = (std::abs(dir.y) < 0.9f) ? dir.cross(Vec3(0, 1, 0)).normalize() : dir.cross(Vec3(1, 0, 0)).normalize();
    Vec3 p2 = dir.cross(p1).normalize();

    int segs = 16;
    for (int i = 0; i < segs; ++i) {
        float a1 = (float)i * (6.2831853f / (float)segs);
        float a2 = (float)(i + 1) * (6.2831853f / (float)segs);
        Vec3 pt1 = base + (p1 * std::cos(a1) + p2 * std::sin(a1)) * radius;
        Vec3 pt2 = base + (p1 * std::cos(a2) + p2 * std::sin(a2)) * radius;

        v.push_back({tip.x, tip.y, tip.z, r, g, b, 1.0f});
        v.push_back({pt1.x, pt1.y, pt1.z, r, g, b, 1.0f});
        v.push_back({pt2.x, pt2.y, pt2.z, r, g, b, 1.0f});
    }
}

void Gizmo::init() {
    lines.clear();
    cones.clear();
    float shaft = 2.0f;

    // 1. محور X الأحمر (خط + سهم)
    lines.push_back({0, 0, 0, 0.95f, 0.2f, 0.2f, 1.0f});
    lines.push_back({shaft, 0, 0, 0.95f, 0.2f, 0.2f, 1.0f});
    addCone(cones, Vec3(shaft, 0, 0), Vec3(1, 0, 0), 0.95f, 0.2f, 0.2f);

    // 2. محور Y الأخضر (خط + سهم للأعلى)
    lines.push_back({0, 0, 0, 0.2f, 0.85f, 0.2f, 1.0f});
    lines.push_back({0, shaft, 0, 0.2f, 0.85f, 0.2f, 1.0f});
    addCone(cones, Vec3(0, shaft, 0), Vec3(0, 1, 0), 0.2f, 0.85f, 0.2f);

    // 3. محور Z الأزرق (خط + سهم)
    lines.push_back({0, 0, 0, 0.2f, 0.45f, 0.98f, 1.0f});
    lines.push_back({0, 0, shaft, 0.2f, 0.45f, 0.98f, 1.0f});
    addCone(cones, Vec3(0, 0, shaft), Vec3(0, 0, 1), 0.2f, 0.45f, 0.98f);
}

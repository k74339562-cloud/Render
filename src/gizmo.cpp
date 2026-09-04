#include "gizmo.h"
#include <cmath>

static void addCone(std::vector<GizmoVertex>& v, const Vec3& base, const Vec3& dir, float r, float g, float b) {
    float coneLen = 0.5f;
    float coneRadius = 0.12f;
    Vec3 tip = base + dir * coneLen;
    Vec3 perp1 = (std::abs(dir.y) < 0.9f) ? dir.cross(Vec3(0, 1, 0)).normalize() : dir.cross(Vec3(1, 0, 0)).normalize();
    Vec3 perp2 = dir.cross(perp1).normalize();

    int segments = 12;
    for (int i = 0; i < segments; ++i) {
        float a1 = (float)i * (6.2831853f / (float)segments);
        float a2 = (float)(i + 1) * (6.2831853f / (float)segments);

        Vec3 p1 = base + (perp1 * std::cos(a1) + perp2 * std::sin(a1)) * coneRadius;
        Vec3 p2 = base + (perp1 * std::cos(a2) + perp2 * std::sin(a2)) * coneRadius;

        // رسم مثلثات رأس السهم المجسم
        v.push_back({tip.x, tip.y, tip.z, r, g, b, 1.0f});
        v.push_back({p1.x, p1.y, p1.z, r, g, b, 1.0f});
        v.push_back({p2.x, p2.y, p2.z, r, g, b, 1.0f});
    }
}

void Gizmo::init() {
    vertices.clear();
    float shaftLen = 1.8f;

    // 1. محور X الأحمر
    vertices.push_back({0, 0, 0, 0.95f, 0.15f, 0.15f, 1.0f});
    vertices.push_back({shaftLen, 0, 0, 0.95f, 0.15f, 0.15f, 1.0f});
    addCone(vertices, Vec3(shaftLen, 0, 0), Vec3(1, 0, 0), 0.95f, 0.15f, 0.15f);

    // 2. محور Y الأخضر
    vertices.push_back({0, 0, 0, 0.20f, 0.88f, 0.20f, 1.0f});
    vertices.push_back({0, shaftLen, 0, 0.20f, 0.88f, 0.20f, 1.0f});
    addCone(vertices, Vec3(0, shaftLen, 0), Vec3(0, 1, 0), 0.20f, 0.88f, 0.20f);

    // 3. محور Z الأزرق
    vertices.push_back({0, 0, 0, 0.20f, 0.45f, 0.98f, 1.0f});
    vertices.push_back({0, 0, shaftLen, 0.20f, 0.45f, 0.98f, 1.0f});
    addCone(vertices, Vec3(0, 0, shaftLen), Vec3(0, 0, 1), 0.20f, 0.45f, 0.98f);
}

// معادلة بلندر لقياس الحجم الثابت في الشاشة مهما اقتربت أو ابتعدت الكاميرا
Mat4 Gizmo::getTransform(const Vec3& cubePos, float camDist) const {
    float scale = std::max(camDist * 0.16f, 0.2f);
    Mat4 m = Mat4::identity();
    m.m[0] = scale;
    m.m[5] = scale;
    m.m[10] = scale;
    m.m[12] = cubePos.x;
    m.m[13] = cubePos.y;
    m.m[14] = cubePos.z;
    return m;
}

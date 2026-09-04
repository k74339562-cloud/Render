#include "gizmo.h"
#include <cmath>

static void addSolidCone(std::vector<GizmoVertex>& v, const Vec3& base, const Vec3& dir, float r, float g, float b) {
    float coneLen = 0.55f;
    float coneRadius = 0.14f;
    Vec3 tip = base + dir * coneLen;

    Vec3 perp1 = (std::abs(dir.y) < 0.9f) ? dir.cross(Vec3(0, 1, 0)).normalize() : dir.cross(Vec3(1, 0, 0)).normalize();
    Vec3 perp2 = dir.cross(perp1).normalize();

    int segments = 16;
    for (int i = 0; i < segments; ++i) {
        float a1 = (float)i * (6.2831853f / (float)segments);
        float a2 = (float)(i + 1) * (6.2831853f / (float)segments);

        Vec3 p1 = base + (perp1 * std::cos(a1) + perp2 * std::sin(a1)) * coneRadius;
        Vec3 p2 = base + (perp1 * std::cos(a2) + perp2 * std::sin(a2)) * coneRadius;

        // مثلثات الجوانب المصمتة لرأس السهم
        v.push_back({tip.x, tip.y, tip.z, r, g, b, 1.0f});
        v.push_back({p1.x, p1.y, p1.z, r, g, b, 1.0f});
        v.push_back({p2.x, p2.y, p2.z, r, g, b, 1.0f});

        // قاعدة المخروط
        v.push_back({base.x, base.y, base.z, r * 0.8f, g * 0.8f, b * 0.8f, 1.0f});
        v.push_back({p2.x, p2.y, p2.z, r * 0.8f, g * 0.8f, b * 0.8f, 1.0f});
        v.push_back({p1.x, p1.y, p1.z, r * 0.8f, g * 0.8f, b * 0.8f, 1.0f});
    }
}

void Gizmo::init() {
    coneVertices.clear();
    float len = 2.2f;

    // 1. رأس محور X الأحمر الأصلي
    addSolidCone(coneVertices, Vec3(len, 0, 0), Vec3(1, 0, 0), 0.95f, 0.20f, 0.20f);

    // 2. رأس محور Y الأخضر الأصلي (يتجه للأعلى الآن)
    addSolidCone(coneVertices, Vec3(0, len, 0), Vec3(0, 1, 0), 0.25f, 0.85f, 0.25f);

    // 3. رأس محور Z الأزرق الأصلي
    addSolidCone(coneVertices, Vec3(0, 0, len), Vec3(0, 0, 1), 0.20f, 0.50f, 0.98f);
}

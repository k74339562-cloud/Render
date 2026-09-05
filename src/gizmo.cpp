#include "gizmo.h"
#include <cmath>

static void addBox(std::vector<GizmoVertex>& v, const Vec3& minP, const Vec3& maxP, float r, float g, float b) {
    Vec3 p[8] = {
        {minP.x, minP.y, minP.z}, {maxP.x, minP.y, minP.z},
        {maxP.x, maxP.y, minP.z}, {minP.x, maxP.y, minP.z},
        {minP.x, minP.y, maxP.z}, {maxP.x, minP.y, maxP.z},
        {maxP.x, maxP.y, maxP.z}, {minP.x, maxP.y, maxP.z}
    };
    int idx[] = {
        0,1,2, 0,2,3,  4,6,5, 4,7,6,  0,4,5, 0,5,1,
        1,5,6, 1,6,2,  2,6,7, 2,7,3,  3,7,4, 3,4,0
    };
    for (int i : idx) {
        v.push_back({p[i].x, p[i].y, p[i].z, r, g, b, 1.0f});
    }
}

static void addCone(std::vector<GizmoVertex>& v, const Vec3& base, const Vec3& dir, float r, float g, float b) {
    float len = 0.42f, radius = 0.11f;
    Vec3 tip = base + dir * len;
    Vec3 perp1 = (std::abs(dir.z) < 0.9f) ? dir.cross(Vec3(0, 0, 1)).normalize() : dir.cross(Vec3(1, 0, 0)).normalize();
    Vec3 perp2 = dir.cross(perp1).normalize();

    int segs = 20;
    for (int i = 0; i < segs; ++i) {
        float a1 = (float)i * (6.2831853f / (float)segs);
        float a2 = (float)(i + 1) * (6.2831853f / (float)segs);
        Vec3 pt1 = base + (perp1 * std::cos(a1) + perp2 * std::sin(a1)) * radius;
        Vec3 pt2 = base + (perp1 * std::cos(a2) + perp2 * std::sin(a2)) * radius;

        // المخروط
        v.push_back({tip.x, tip.y, tip.z, r, g, b, 1.0f});
        v.push_back({pt1.x, pt1.y, pt1.z, r, g, b, 1.0f});
        v.push_back({pt2.x, pt2.y, pt2.z, r, g, b, 1.0f});

        // قاعدة المخروط
        v.push_back({base.x, base.y, base.z, r * 0.75f, g * 0.75f, b * 0.75f, 1.0f});
        v.push_back({pt2.x, pt2.y, pt2.z, r * 0.75f, g * 0.75f, b * 0.75f, 1.0f});
        v.push_back({pt1.x, pt1.y, pt1.z, r * 0.75f, g * 0.75f, b * 0.75f, 1.0f});
    }
}

void Gizmo::init() {
    vertices.clear();
    float shaftLen = 1.65f;
    float w = 0.022f;

    // 1. محور X الأحمر (+X يمين) كود بلندر الأصلي #EA3C53
    addBox(vertices, Vec3(0, -w, -w), Vec3(shaftLen, w, w), 0.92f, 0.23f, 0.32f);
    addCone(vertices, Vec3(shaftLen, 0, 0), Vec3(1, 0, 0), 0.92f, 0.23f, 0.32f);

    // 2. محور Y الأخضر (+Y للأمام) كود بلندر الأصلي #82C823
    addBox(vertices, Vec3(-w, 0, -w), Vec3(w, shaftLen, w), 0.51f, 0.78f, 0.14f);
    addCone(vertices, Vec3(0, shaftLen, 0), Vec3(0, 1, 0), 0.51f, 0.78f, 0.14f);

    // 3. محور Z الأزرق (+Z للأعلى) كود بلندر الأصلي #2C85F6
    addBox(vertices, Vec3(-w, -w, 0), Vec3(w, w, shaftLen), 0.17f, 0.52f, 0.96f);
    addCone(vertices, Vec3(0, 0, shaftLen), Vec3(0, 0, 1), 0.17f, 0.52f, 0.96f);

    // مكعب مركزي أبيض صغير يمثل نقطة الأصل
    float cw = 0.045f;
    addBox(vertices, Vec3(-cw, -cw, -cw), Vec3(cw, cw, cw), 0.95f, 0.95f, 0.95f);
}

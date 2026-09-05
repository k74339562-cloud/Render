#include "gizmo.h"
#include <cmath>

static void addBox(std::vector<GizmoVertex>& v, const Vec3& minP, const Vec3& maxP, float r, float g, float b, float a = 1.0f) {
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
        v.push_back({p[i].x, p[i].y, p[i].z, r, g, b, a});
    }
}

static void addCone(std::vector<GizmoVertex>& v, const Vec3& base, const Vec3& dir, float r, float g, float b) {
    float len = 0.38f, radius = 0.095f;
    Vec3 tip = base + dir * len;
    Vec3 perp1 = (std::abs(dir.z) < 0.9f) ? dir.cross(Vec3(0, 0, 1)).normalize() : dir.cross(Vec3(1, 0, 0)).normalize();
    Vec3 perp2 = dir.cross(perp1).normalize();

    int segs = 24;
    for (int i = 0; i < segs; ++i) {
        float a1 = (float)i * (6.2831853f / (float)segs);
        float a2 = (float)(i + 1) * (6.2831853f / (float)segs);
        Vec3 pt1 = base + (perp1 * std::cos(a1) + perp2 * std::sin(a1)) * radius;
        Vec3 pt2 = base + (perp1 * std::cos(a2) + perp2 * std::sin(a2)) * radius;

        // مخروط السهم
        v.push_back({tip.x, tip.y, tip.z, r, g, b, 1.0f});
        v.push_back({pt1.x, pt1.y, pt1.z, r, g, b, 1.0f});
        v.push_back({pt2.x, pt2.y, pt2.z, r, g, b, 1.0f});

        // قاعدة المخروط
        v.push_back({base.x, base.y, base.z, r * 0.8f, g * 0.8f, b * 0.8f, 1.0f});
        v.push_back({pt2.x, pt2.y, pt2.z, r * 0.8f, g * 0.8f, b * 0.8f, 1.0f});
        v.push_back({pt1.x, pt1.y, pt1.z, r * 0.8f, g * 0.8f, b * 0.8f, 1.0f});
    }
}

void Gizmo::init() {
    vertices.clear();
    float shaftLen = 1.65f;
    float w = 0.020f;

    // ألوان محاور بلندر الرسمية بدقة الهيكس
    // X = #EA3C53 (أحمر) | Y = #82C823 (أخضر) | Z = #2C85F6 (أزرق)
    const float xr = 0.92f, xg = 0.23f, xb = 0.32f;
    const float yr = 0.51f, yg = 0.78f, yb = 0.14f;
    const float zr = 0.17f, zg = 0.52f, zb = 0.96f;

    // 1. عمود ومخروط المحور X (+X)
    addBox(vertices, Vec3(0, -w, -w), Vec3(shaftLen, w, w), xr, xg, xb);
    addCone(vertices, Vec3(shaftLen, 0, 0), Vec3(1, 0, 0), xr, xg, xb);

    // 2. عمود ومخروط المحور Y (+Y)
    addBox(vertices, Vec3(-w, 0, -w), Vec3(w, shaftLen, w), yr, yg, yb);
    addCone(vertices, Vec3(0, shaftLen, 0), Vec3(0, 1, 0), yr, yg, yb);

    // 3. عمود ومخروط المحور Z (+Z)
    addBox(vertices, Vec3(-w, -w, 0), Vec3(w, w, shaftLen), zr, zg, zb);
    addCone(vertices, Vec3(0, 0, shaftLen), Vec3(0, 0, 1), zr, zg, zb);

    // 4. مقابض المسطحات الثنائية (Planar Handles) المميزة لجزمو بلندر
    float pStart = 0.55f;
    float pSize = 0.28f;
    float pThick = 0.005f;

    // مسطح XY (المربع الأزرق الموازي للأرضية)
    addBox(vertices, Vec3(pStart, pStart, -pThick), Vec3(pStart + pSize, pStart + pSize, pThick), zr, zg, zb, 0.85f);

    // مسطح XZ (المربع الأخضر الرأسي)
    addBox(vertices, Vec3(pStart, -pThick, pStart), Vec3(pStart + pSize, pThick, pStart + pSize), yr, yg, yb, 0.85f);

    // مسطح YZ (المربع الأحمر الجانبي)
    addBox(vertices, Vec3(-pThick, pStart, pStart), Vec3(pThick, pStart + pSize, pStart + pSize), xr, xg, xb, 0.85f);

    // 5. مكعب الارتكاز المركزي الأبيض الناصع (Center Pivot)
    float cw = 0.040f;
    addBox(vertices, Vec3(-cw, -cw, -cw), Vec3(cw, cw, cw), 1.0f, 1.0f, 1.0f, 1.0f);
}

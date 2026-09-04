#include "gizmo.h"

struct GizmoVertex {
    float x, y, z;
    float r, g, b;
};

void Gizmo::init() {
    float len = 2.4f;
    GizmoVertex lines[] = {
        // محور X الأحمر
        {0, 0, 0, 0.95f, 0.15f, 0.15f}, {len, 0, 0, 0.95f, 0.15f, 0.15f},
        // محور Y الأخضر
        {0, 0, 0, 0.20f, 0.88f, 0.20f}, {0, len, 0, 0.20f, 0.88f, 0.20f},
        // محور Z الأزرق
        {0, 0, 0, 0.20f, 0.45f, 0.98f}, {0, 0, len, 0.20f, 0.45f, 0.98f}
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
}

void Gizmo::render(const Mat4& vp) {
    (void)vp;
    glDisable(GL_DEPTH_TEST); // إظهار الجزمو دائماً فوق المكعب
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)(3 * sizeof(float)));

    glLineWidth(4.0f);
    glDrawArrays(GL_LINES, 0, 6);
    glEnable(GL_DEPTH_TEST);
}

void Gizmo::checkHit(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& cubePos) {
    // رياضيات تفاعل الأشعة
    (void)rayOrigin; (void)rayDir; (void)cubePos;
}

#include "gles_renderer.h"
#include <cmath>
#include <vector>

struct VertexLine { float x, y, z; float r, g, b, a; };

static float distToScreenSegment(float tx, float ty, const Vec2& p0, const Vec2& p1) {
    float vx = p1.x - p0.x, vy = p1.y - p0.y;
    float wx = tx - p0.x,   wy = ty - p0.y;
    float c1 = wx * vx + wy * vy;
    if (c1 <= 0.0f) return std::sqrt(wx * wx + wy * wy);
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        float dx = tx - p1.x, dy = ty - p1.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    float b = c1 / c2;
    float px = p0.x + b * vx, py = p0.y + b * vy;
    float dx = tx - px, dy = ty - py;
    return std::sqrt(dx * dx + dy * dy);
}

GizmoAxis GLESRenderer::testGizmoHit(float touchX, float touchY, float screenW, float screenH) {
    if (!isGizmoVisible) return AXIS_NONE;

    Vec3 gPos = mesh.getActiveGizmoPosition();
    Mat4 gOrient = mesh.getActiveGizmoOrientation();
    Vec3 dirX = Vec3(gOrient.m[0], gOrient.m[1], gOrient.m[2]);
    Vec3 dirY = Vec3(gOrient.m[4], gOrient.m[5], gOrient.m[6]);
    Vec3 dirZ = Vec3(gOrient.m[8], gOrient.m[9], gOrient.m[10]);

    Vec2 pCenter = camera.projectToScreen(gPos, screenW, screenH);
    float distCenter = std::sqrt((touchX - pCenter.x) * (touchX - pCenter.x) + (touchY - pCenter.y) * (touchY - pCenter.y));
    if (distCenter < 38.0f) return AXIS_CENTER;

    float shaftLen = 1.8f;
    Vec2 pX = camera.projectToScreen(gPos + dirX * shaftLen, screenW, screenH);
    Vec2 pY = camera.projectToScreen(gPos + dirY * shaftLen, screenW, screenH);
    Vec2 pZ = camera.projectToScreen(gPos + dirZ * shaftLen, screenW, screenH);

    float dX = distToScreenSegment(touchX, touchY, pCenter, pX);
    float dY = distToScreenSegment(touchX, touchY, pCenter, pY);
    float dZ = distToScreenSegment(touchX, touchY, pCenter, pZ);

    float minDist = 50.0f;
    GizmoAxis hit = AXIS_NONE;

    if (dZ < minDist) { minDist = dZ; hit = AXIS_Z; }
    if (dY < minDist) { minDist = dY; hit = AXIS_Y; }
    if (dX < minDist) { minDist = dX; hit = AXIS_X; }

    return hit;
}

void GLESRenderer::dragGizmo(float dx, float dy, float screenW, float screenH) {
    if (activeAxis == AXIS_NONE || !isGizmoVisible) return;

    Vec3 gPos = mesh.getActiveGizmoPosition();
    Mat4 gOrient = mesh.getActiveGizmoOrientation();
    float camDist = (camera.getPosition() - gPos).length();
    float worldUnitsPerPixel = (camDist * 0.0015f);

    if (activeAxis == AXIS_CENTER) {
        float cosY = std::cos(camera.yaw), sinY = std::sin(camera.yaw);
        Vec3 camRight = {-cosY, sinY, 0.0f};
        Vec3 camUp = {-sinY * std::sin(camera.pitch), -cosY * std::sin(camera.pitch), std::cos(camera.pitch)};
        Vec3 deltaMove = (camRight * (dx * worldUnitsPerPixel)) + (camUp * (-dy * worldUnitsPerPixel));
        mesh.position = mesh.position + deltaMove;
        return;
    }

    Vec3 axisDir3D = {0, 0, 0};
    if (activeAxis == AXIS_X) axisDir3D = Vec3(gOrient.m[0], gOrient.m[1], gOrient.m[2]);
    if (activeAxis == AXIS_Y) axisDir3D = Vec3(gOrient.m[4], gOrient.m[5], gOrient.m[6]);
    if (activeAxis == AXIS_Z) axisDir3D = Vec3(gOrient.m[8], gOrient.m[9], gOrient.m[10]);

    Vec2 pCenter = camera.projectToScreen(gPos, screenW, screenH);
    Vec2 pTip    = camera.projectToScreen(gPos + axisDir3D, screenW, screenH);

    float screenDirX = pTip.x - pCenter.x;
    float screenDirY = pTip.y - pCenter.y;
    float len = std::sqrt(screenDirX * screenDirX + screenDirY * screenDirY);
    if (len < 0.001f) return;

    screenDirX /= len; screenDirY /= len;
    float dotMove = (dx * screenDirX) + (dy * screenDirY);
    mesh.position = mesh.position + (axisDir3D * (dotMove * worldUnitsPerPixel));
}

void GLESRenderer::handleTapSelection(float touchX, float touchY, float screenW, float screenH) {
    Ray ray = camera.getScreenRay(touchX, touchY, screenW, screenH);
    float dist = 0.0f;

    if (mesh.selectMode == SelectionMode::OBJECT) {
        if (mesh.pickObject(ray, dist)) {
            mesh.isObjectSelected = true;
            isGizmoVisible = true;
        } else {
            mesh.deselectAll();
            isGizmoVisible = false;
        }
    } else if (mesh.selectMode == SelectionMode::FACE) {
        int fIdx = mesh.pickFace(ray, dist);
        if (fIdx != -1) {
            mesh.selectedFaceIdx = fIdx;
            isGizmoVisible = true;
        } else {
            mesh.deselectAll();
            isGizmoVisible = false;
        }
    } else if (mesh.selectMode == SelectionMode::EDGE) {
        int eIdx = mesh.pickEdge(ray, 0.25f);
        if (eIdx != -1) {
            mesh.selectedEdgeIdx = eIdx;
            isGizmoVisible = true;
        } else {
            mesh.deselectAll();
            isGizmoVisible = false;
        }
    } else if (mesh.selectMode == SelectionMode::VERTEX) {
        int vIdx = mesh.pickVertex(ray, 0.25f);
        if (vIdx != -1) {
            mesh.selectedVertexIdx = vIdx;
            isGizmoVisible = true;
        } else {
            mesh.deselectAll();
            isGizmoVisible = false;
        }
    }

    mesh.rebuildBuffers();
}

bool GLESRenderer::init(ANativeWindow* window) {
    if (!engine.init(window)) return false;

    mesh.initDefaultCube();

    // بناء شبكة بلندر ثلاثية الأبعاد
    std::vector<VertexLine> gridLines;
    int gridSize = 20;
    float maxDist = (float)gridSize;

    for (int i = -gridSize; i <= gridSize; ++i) {
        float fi = (float)i;
        float d = std::abs(fi) / maxDist;
        float alpha = std::pow(1.0f - d, 1.8f) * 0.40f;

        if (i == 0) {
            gridLines.push_back({-maxDist, 0.0f, -1.0f, 0.92f, 0.23f, 0.32f, 0.95f});
            gridLines.push_back({ maxDist, 0.0f, -1.0f, 0.92f, 0.23f, 0.32f, 0.95f});
            gridLines.push_back({0.0f, -maxDist, -1.0f, 0.51f, 0.78f, 0.14f, 0.95f});
            gridLines.push_back({0.0f,  maxDist, -1.0f, 0.51f, 0.78f, 0.14f, 0.95f});
            continue;
        }

        float gc = 0.29f;
        gridLines.push_back({-maxDist, fi, -1.0f, gc, gc, gc, alpha});
        gridLines.push_back({ maxDist, fi, -1.0f, gc, gc, gc, alpha});
        gridLines.push_back({fi, -maxDist, -1.0f, gc, gc, gc, alpha});
        gridLines.push_back({fi,  maxDist, -1.0f, gc, gc, gc, alpha});
    }
    gridVertexCount = (uint32_t)gridLines.size();

    glGenVertexArrays(1, &gridVao);
    glBindVertexArray(gridVao);

    glGenBuffers(1, &gridVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
    glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(VertexLine), gridLines.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexLine), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexLine), (void*)(3 * sizeof(float)));

    // بناء مجسم الجزمو
    gizmo.init();
    gizmoVertexCount = (uint32_t)gizmo.vertices.size();

    glGenVertexArrays(1, &gizmoVao);
    glBindVertexArray(gizmoVao);

    glGenBuffers(1, &gizmoVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gizmoVbo);
    glBufferData(GL_ARRAY_BUFFER, gizmo.vertices.size() * sizeof(GizmoVertex), gizmo.vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
    return true;
}

void GLESRenderer::cleanup() {
    if (gridVao) { glDeleteVertexArrays(1, &gridVao); gridVao = 0; }
    if (gridVbo) { glDeleteBuffers(1, &gridVbo); gridVbo = 0; }

    if (gizmoVao) { glDeleteVertexArrays(1, &gizmoVao); gizmoVao = 0; }
    if (gizmoVbo) { glDeleteBuffers(1, &gizmoVbo); gizmoVbo = 0; }

    mesh.cleanup();
    engine.cleanup();
}

void GLESRenderer::renderFrame() {
    float screenW = (float)engine.width;
    float screenH = (float)engine.height;

    Mat4 v = camera.getViewMatrix();
    Mat4 p = camera.getProjectionMatrix(screenW, screenH);
    Mat4 vp = p * v;
    Vec3 eye = camera.getPosition();

    engine.beginFrame(vp, eye);

    // 1. رسم شبكة الأرضية
    engine.useLineProgram(Mat4::identity());
    glBindVertexArray(gridVao);
    glDrawArrays(GL_LINES, 0, gridVertexCount);

    // 2. رسم المجسم
    mesh.draw(engine);

    // 3. رسم الجزمو (دائماً في المقدمة مثل بلندر بدون حجب بالعمق)
    if (isGizmoVisible && gizmoVao) {
        glClear(GL_DEPTH_BUFFER_BIT); // مسح بافر العمق ليظهر الجزمو بوضوح فوق المجسم
        Mat4 gizmoTransform = mesh.getActiveGizmoOrientation();
        engine.useLineProgram(gizmoTransform);
        glBindVertexArray(gizmoVao);
        glDrawArrays(GL_TRIANGLES, 0, gizmoVertexCount);
    }

    glBindVertexArray(0);
    engine.endFrame();
}

#include "vulkan_renderer.h"
#include <cmath>

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

GizmoAxis VulkanRenderer::testGizmoHit(float touchX, float touchY, float screenW, float screenH) {
    Vec2 pCenter = camera.projectToScreen(box.position, screenW, screenH);

    float distCenter = std::sqrt((touchX - pCenter.x) * (touchX - pCenter.x) + 
                                 (touchY - pCenter.y) * (touchY - pCenter.y));
    if (distCenter < 38.0f) {
        return AXIS_CENTER;
    }

    float shaftLen = 1.8f;
    Vec2 pX = camera.projectToScreen(box.position + Vec3(shaftLen, 0, 0), screenW, screenH);
    Vec2 pY = camera.projectToScreen(box.position + Vec3(0, shaftLen, 0), screenW, screenH);
    Vec2 pZ = camera.projectToScreen(box.position + Vec3(0, 0, shaftLen), screenW, screenH);

    float dX = distToScreenSegment(touchX, touchY, pCenter, pX);
    float dY = distToScreenSegment(touchX, touchY, pCenter, pY);
    float dZ = distToScreenSegment(touchX, touchY, pCenter, pZ);

    float threshold = 50.0f;
    GizmoAxis hit = AXIS_NONE;
    float minDist = threshold;

    if (dZ < minDist) { minDist = dZ; hit = AXIS_Z; }
    if (dY < minDist) { minDist = dY; hit = AXIS_Y; }
    if (dX < minDist) { minDist = dX; hit = AXIS_X; }

    return hit;
}

void VulkanRenderer::dragGizmo(float dx, float dy, float screenW, float screenH) {
    if (activeAxis == AXIS_NONE) return;

    float camDist = (camera.getPosition() - box.position).length();
    float worldUnitsPerPixel = (camDist * 0.0015f);

    if (activeAxis == AXIS_CENTER) {
        float cosY = std::cos(camera.yaw), sinY = std::sin(camera.yaw);
        Vec3 camRight = {-cosY, sinY, 0.0f};
        Vec3 camUp = {-sinY * std::sin(camera.pitch), -cosY * std::sin(camera.pitch), std::cos(camera.pitch)};

        Vec3 deltaMove = (camRight * (dx * worldUnitsPerPixel)) + 
                         (camUp * (-dy * worldUnitsPerPixel));

        box.position = box.position + deltaMove;
        return;
    }

    Vec3 axisDir3D = {0, 0, 0};
    if (activeAxis == AXIS_X) axisDir3D = {1.0f, 0.0f, 0.0f};
    if (activeAxis == AXIS_Y) axisDir3D = {0.0f, 1.0f, 0.0f};
    if (activeAxis == AXIS_Z) axisDir3D = {0.0f, 0.0f, 1.0f};

    Vec2 pCenter = camera.projectToScreen(box.position, screenW, screenH);
    Vec2 pTip    = camera.projectToScreen(box.position + axisDir3D, screenW, screenH);

    float screenDirX = pTip.x - pCenter.x;
    float screenDirY = pTip.y - pCenter.y;
    float len = std::sqrt(screenDirX * screenDirX + screenDirY * screenDirY);
    if (len < 0.001f) return;

    screenDirX /= len;
    screenDirY /= len;

    float dotMove = (dx * screenDirX) + (dy * screenDirY);
    box.position = box.position + (axisDir3D * (dotMove * worldUnitsPerPixel));
}

bool VulkanRenderer::init(ANativeWindow* window) {
    if (!engine.init(window)) return false;

    // تهيئة المكعب عبر مكتبة الرندر
    box.init(engine);

    // شبكة الأرضية
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

    engine.createBuffer(gridLines.size() * sizeof(VertexLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gridVbo, gridVboMemory);
    void* gLineData; vkMapMemory(engine.device, gridVboMemory, 0, gridLines.size() * sizeof(VertexLine), 0, &gLineData);
    memcpy(gLineData, gridLines.data(), gridLines.size() * sizeof(VertexLine)); vkUnmapMemory(engine.device, gridVboMemory);

    // تهيئة الجزمو
    gizmo.init();
    gizmoVertexCount = (uint32_t)gizmo.vertices.size();
    engine.createBuffer(gizmo.vertices.size() * sizeof(GizmoVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gizmoVbo, gizmoVboMemory);
    void* gizmoData; vkMapMemory(engine.device, gizmoVboMemory, 0, gizmo.vertices.size() * sizeof(GizmoVertex), 0, &gizmoData);
    memcpy(gizmoData, gizmo.vertices.data(), gizmo.vertices.size() * sizeof(GizmoVertex)); vkUnmapMemory(engine.device, gizmoVboMemory);

    return true;
}

void VulkanRenderer::cleanup() {
    box.cleanup(engine);
    engine.destroyBuffer(gizmoVbo, gizmoVboMemory);
    engine.destroyBuffer(gridVbo, gridVboMemory);
    engine.cleanup();
}

void VulkanRenderer::renderFrame() {
    float screenW = (float)engine.swapchainExtent.width;
    float screenH = (float)engine.swapchainExtent.height;

    Mat4 v = camera.getViewMatrix();
    Mat4 p = camera.getProjectionMatrix(screenW, screenH);
    Vec3 eye = camera.getPosition();

    // 1. فتح الفريم
    if (!engine.beginFrame(v, p, eye)) return;

    // 2. رسم شبكة الأرضية الثابتة
    engine.drawLines(gridVbo, gridVertexCount, Mat4::identity());

    // 3. رسم المكعب المستقل مع حوافه البرتقالية
    box.draw(engine);

    // 4. رسم الجزمو المتحرك مع المكعب
    engine.drawGizmo(gizmoVbo, gizmoVertexCount, box.getModelMatrix());

    // 5. إنهاء الفريم وإرساله للشاشة
    engine.endFrame();
}

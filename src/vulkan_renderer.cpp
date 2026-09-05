#include "vulkan_renderer.h"
#include <cmath>
#include <cstring>

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
    if (!isGizmoVisible) return AXIS_NONE;

    Vec3 gPos = mesh.getActiveGizmoPosition();
    Mat4 gOrient = mesh.getActiveGizmoOrientation();
    Vec3 dirX = Vec3(gOrient.m[0], gOrient.m[1], gOrient.m[2]);
    Vec3 dirY = Vec3(gOrient.m[4], gOrient.m[5], gOrient.m[6]);
    Vec3 dirZ = Vec3(gOrient.m[8], gOrient.m[9], gOrient.m[10]);

    Vec2 pCenter = camera.projectToScreen(gPos, screenW, screenH);

    float distCenter = std::sqrt((touchX - pCenter.x) * (touchX - pCenter.x) + 
                                 (touchY - pCenter.y) * (touchY - pCenter.y));
    if (distCenter < 38.0f) {
        return AXIS_CENTER;
    }

    float shaftLen = 1.8f;
    Vec2 pX = camera.projectToScreen(gPos + dirX * shaftLen, screenW, screenH);
    Vec2 pY = camera.projectToScreen(gPos + dirY * shaftLen, screenW, screenH);
    Vec2 pZ = camera.projectToScreen(gPos + dirZ * shaftLen, screenW, screenH);

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
    if (activeAxis == AXIS_NONE || !isGizmoVisible) return;

    Vec3 gPos = mesh.getActiveGizmoPosition();
    Mat4 gOrient = mesh.getActiveGizmoOrientation();
    float camDist = (camera.getPosition() - gPos).length();
    float worldUnitsPerPixel = (camDist * 0.0015f);

    if (activeAxis == AXIS_CENTER) {
        float cosY = std::cos(camera.yaw), sinY = std::sin(camera.yaw);
        Vec3 camRight = {-cosY, sinY, 0.0f};
        Vec3 camUp = {-sinY * std::sin(camera.pitch), -cosY * std::sin(camera.pitch), std::cos(camera.pitch)};

        Vec3 deltaMove = (camRight * (dx * worldUnitsPerPixel)) + 
                         (camUp * (-dy * worldUnitsPerPixel));

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

    screenDirX /= len;
    screenDirY /= len;

    float dotMove = (dx * screenDirX) + (dy * screenDirY);
    mesh.position = mesh.position + (axisDir3D * (dotMove * worldUnitsPerPixel));
}

// فحص النقر على أزرار واجهة بلندر في أعلى الشاشة بدقة
bool VulkanRenderer::handleUITouch(float touchX, float touchY, float screenW, float screenH) {
    if (touchY < 20.0f || touchY > 110.0f) return false;

    // زر التبديل الرئيسي بين وضع الكائن ووضع التعديل
    if (touchX >= 20.0f && touchX <= 260.0f) {
        if (mesh.selectMode == SelectionMode::OBJECT) {
            mesh.selectMode = SelectionMode::VERTEX;
            mesh.isObjectSelected = false;
            mesh.selectedVertexIdx = 0;
            isGizmoVisible = true;
        } else {
            mesh.selectMode = SelectionMode::OBJECT;
            mesh.isObjectSelected = true;
            isGizmoVisible = true;
        }
        mesh.rebuildBuffers(engine);
        updateUIBuffers();
        return true;
    }

    // أزرار نمط التعديل
    if (mesh.selectMode != SelectionMode::OBJECT) {
        // زر النقاط
        if (touchX >= 280.0f && touchX <= 390.0f) {
            mesh.selectMode = SelectionMode::VERTEX;
            mesh.selectedVertexIdx = 0;
            isGizmoVisible = true;
            mesh.rebuildBuffers(engine);
            updateUIBuffers();
            return true;
        }
        // زر الحواف
        if (touchX >= 410.0f && touchX <= 520.0f) {
            mesh.selectMode = SelectionMode::EDGE;
            mesh.selectedEdgeIdx = 0;
            isGizmoVisible = true;
            mesh.rebuildBuffers(engine);
            updateUIBuffers();
            return true;
        }
        // زر الأوجه
        if (touchX >= 540.0f && touchX <= 650.0f) {
            mesh.selectMode = SelectionMode::FACE;
            mesh.selectedFaceIdx = 1;
            isGizmoVisible = true;
            mesh.rebuildBuffers(engine);
            updateUIBuffers();
            return true;
        }
    }
    return false;
}

// بناء وتحديث أزرار الواجهة الثنائية الأبعاد
void VulkanRenderer::updateUIBuffers() {
    float screenW = (float)engine.swapchainExtent.width;
    float screenH = (float)engine.swapchainExtent.height;
    if (screenW <= 0 || screenH <= 0) return;

    std::vector<VertexLine> uiLines;

    auto addBox2D = [&](float px0, float py0, float px1, float py1, float r, float g, float b, float a) {
        // تحويل بكسلات الشاشة إلى مساحة NDC الصافية [-1.0, 1.0]
        float x0 = (2.0f * px0) / screenW - 1.0f;
        float y0 = 1.0f - (2.0f * py0) / screenH;
        float x1 = (2.0f * px1) / screenW - 1.0f;
        float y1 = 1.0f - (2.0f * py1) / screenH;

        uiLines.push_back({x0, y0, 0.0f, r, g, b, a}); uiLines.push_back({x1, y0, 0.0f, r, g, b, a});
        uiLines.push_back({x1, y0, 0.0f, r, g, b, a}); uiLines.push_back({x1, y1, 0.0f, r, g, b, a});
        uiLines.push_back({x1, y1, 0.0f, r, g, b, a}); uiLines.push_back({x0, y1, 0.0f, r, g, b, a});
        uiLines.push_back({x0, y1, 0.0f, r, g, b, a}); uiLines.push_back({x0, y0, 0.0f, r, g, b, a});
    };

    bool isEdit = (mesh.selectMode != SelectionMode::OBJECT);

    // 1. زر التبديل الرئيسي
    if (isEdit) {
        addBox2D(20, 24, 260, 94, 0.28f, 0.45f, 0.70f, 1.0f); // أزرق بلندر
        addBox2D(24, 28, 256, 90, 0.28f, 0.45f, 0.70f, 0.5f);
    } else {
        addBox2D(20, 24, 260, 94, 0.91f, 0.42f, 0.10f, 1.0f); // برتقالي بلندر للكائن
        addBox2D(24, 28, 256, 90, 0.91f, 0.42f, 0.10f, 0.5f);
    }

    // 2. أزرار الأنماط الفرعية
    if (isEdit) {
        // زر النقاط
        float vr = (mesh.selectMode == SelectionMode::VERTEX) ? 1.0f : 0.45f;
        float vg = (mesh.selectMode == SelectionMode::VERTEX) ? 0.7f : 0.45f;
        addBox2D(280, 24, 390, 94, vr, vg, 0.15f, 1.0f);
        addBox2D(330, 54, 340, 64, 1, 1, 1, 1); // أيقونة النقطة

        // زر الحواف
        float er = (mesh.selectMode == SelectionMode::EDGE) ? 1.0f : 0.45f;
        float eg = (mesh.selectMode == SelectionMode::EDGE) ? 0.7f : 0.45f;
        addBox2D(410, 24, 520, 94, er, eg, 0.15f, 1.0f);
        addBox2D(445, 74, 485, 44, 1, 1, 1, 1); // أيقونة الخط

        // زر الأوجه
        float fr = (mesh.selectMode == SelectionMode::FACE) ? 1.0f : 0.45f;
        float fg = (mesh.selectMode == SelectionMode::FACE) ? 0.7f : 0.45f;
        addBox2D(540, 24, 650, 94, fr, fg, 0.15f, 1.0f);
        addBox2D(580, 44, 610, 74, 1, 1, 1, 1); // أيقونة المربع
    }

    if (uiVbo != VK_NULL_HANDLE) {
        engine.destroyBuffer(uiVbo, uiVboMemory);
        uiVbo = VK_NULL_HANDLE;
    }

    uiVertexCount = (uint32_t)uiLines.size();
    if (uiVertexCount > 0) {
        engine.createBuffer(uiLines.size() * sizeof(VertexLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uiVbo, uiVboMemory);
        void* uData; vkMapMemory(engine.device, uiVboMemory, 0, uiLines.size() * sizeof(VertexLine), 0, &uData);
        memcpy(uData, uiLines.data(), uiLines.size() * sizeof(VertexLine)); vkUnmapMemory(engine.device, uiVboMemory);
    }
}

void VulkanRenderer::handleTapSelection(float touchX, float touchY, float screenW, float screenH) {
    Ray ray = camera.getScreenRay(touchX, touchY, screenW, screenH);
    float dist = 0.0f;

    if (mesh.selectMode == SelectionMode::OBJECT) {
        if (mesh.pickObject(ray, dist)) {
            mesh.isObjectSelected = true;
            isGizmoVisible = true;
        } else {
            // النقر في الفراغ يلغي التحديد ويخفي الجزمو
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

    mesh.rebuildBuffers(engine);
}

bool VulkanRenderer::init(ANativeWindow* window) {
    if (!engine.init(window)) return false;

    mesh.initDefaultCube(engine);

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

    updateUIBuffers();
    return true;
}

void VulkanRenderer::cleanup() {
    if (uiVbo) engine.destroyBuffer(uiVbo, uiVboMemory);
    mesh.cleanup(engine);
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

    if (!engine.beginFrame(v, p, eye)) return;

    // 1. رسم شبكة الأرضية
    engine.drawLines(gridVbo, gridVertexCount, Mat4::identity());

    // 2. رسم المجسم وعناصره
    mesh.draw(engine);

    // 3. رسم الجزمو الديناميكي إذا كان هناك عنصر محدد
    if (isGizmoVisible) {
        Mat4 gizmoTransform = mesh.getActiveGizmoOrientation();
        engine.drawGizmo(gizmoVbo, gizmoVertexCount, gizmoTransform);
    }

    // 4. الحل الهندسي: إلغاء مصفوفة الكاميرا لتثبيت الأزرار على زجاج الشاشة ثنائية الأبعاد
    if (uiVbo != VK_NULL_HANDLE && uiVertexCount > 0) {
        Mat4 invVP = (p * v).inverse(); // يلغي تأثير الكاميرا تماماً لتظهر الأزرار 2D بحجمها الحقيقي
        engine.drawOverlayLines(uiVbo, uiVertexCount, invVP);
    }

    engine.endFrame();
}

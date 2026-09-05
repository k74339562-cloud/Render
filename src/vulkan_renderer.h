#pragma once
#include <android/native_window.h>
#include <vector>
#include "camera.h"
#include "gizmo.h"
#include "box.h"
#include "render_engine.h"

enum GizmoAxis {
    AXIS_NONE = 0,
    AXIS_X,
    AXIS_Y,
    AXIS_Z,
    AXIS_CENTER
};

class VulkanRenderer {
public:
    RenderEngine engine; // المحرك المستقل المنفصل بالكامل
    Camera camera;
    Gizmo gizmo;
    Box box;

    VkBuffer gridVbo = VK_NULL_HANDLE;
    VkDeviceMemory gridVboMemory = VK_NULL_HANDLE;
    uint32_t gridVertexCount = 0;

    VkBuffer gizmoVbo = VK_NULL_HANDLE;
    VkDeviceMemory gizmoVboMemory = VK_NULL_HANDLE;
    uint32_t gizmoVertexCount = 0;

    GizmoAxis activeAxis = AXIS_NONE;

    bool init(ANativeWindow* window);
    void cleanup();
    void renderFrame();

    GizmoAxis testGizmoHit(float touchX, float touchY, float screenW, float screenH);
    void dragGizmo(float dx, float dy, float screenW, float screenH);
};

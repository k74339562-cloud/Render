#pragma once
#include <android/native_window.h>
#include "gles_engine.h"
#include "camera.h"
#include "gizmo.h"
#include "mesh_data.h"

enum GizmoAxis {
    AXIS_NONE = 0,
    AXIS_X,
    AXIS_Y,
    AXIS_Z,
    AXIS_CENTER
};

class GLESRenderer {
public:
    GLESEngine engine;
    Camera camera;
    Gizmo gizmo;
    MeshData mesh;

    GLuint gridVao = 0, gridVbo = 0;
    uint32_t gridVertexCount = 0;

    GLuint gizmoVao = 0, gizmoVbo = 0;
    uint32_t gizmoVertexCount = 0;

    GizmoAxis activeAxis = AXIS_NONE;
    bool isGizmoVisible = true;

    bool init(ANativeWindow* window);
    void cleanup();
    void renderFrame();

    GizmoAxis testGizmoHit(float touchX, float touchY, float screenW, float screenH);
    void dragGizmo(float dx, float dy, float screenW, float screenH);
    void handleTapSelection(float touchX, float touchY, float screenW, float screenH);
};

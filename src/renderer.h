#pragma once
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "camera.h"
#include "mesh.h"
#include "gizmo.h"

class Renderer {
public:
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int width = 0, height = 0;

    Camera camera;
    Mesh mesh;
    Gizmo gizmo;

    GLuint cubeShader = 0;
    GLuint lineShader = 0;

    bool initEGL(ANativeWindow* window);
    void destroyEGL();
    void initShaders();
    void renderFrame();
};

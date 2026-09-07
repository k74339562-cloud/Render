#pragma once
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include "math_3d.h"

class GLESEngine {
public:
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;

    int width = 0;
    int height = 0;

    // برامج الشيدر
    GLuint meshProgram = 0;
    GLuint lineProgram = 0;

    // مواقع الـ Uniforms في شيدر المجسم
    GLint uMeshViewProj = -1;
    GLint uMeshModel = -1;
    GLint uMeshCamPos = -1;

    // مواقع الـ Uniforms في شيدر الخطوط
    GLint uLineViewProj = -1;
    GLint uLineModel = -1;

    bool init(ANativeWindow* window);
    void cleanup();

    void beginFrame(const Mat4& viewProj, const Vec3& camPos);
    void endFrame();

    void useMeshProgram(const Mat4& model);
    void useLineProgram(const Mat4& model);

private:
    Mat4 currentViewProj;
    Vec3 currentCamPos;

    GLuint buildProgram(const char* vertSrc, const char* fragSrc);
    GLuint compileShader(GLenum type, const char* src);
};

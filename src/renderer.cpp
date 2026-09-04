#include "renderer.h"
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "RenderEngine", __VA_ARGS__))

// شيدر بلندر الأصلي (Studio Clay + Rim Light)
static const char* VS_CUBE = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNorm;
uniform mat4 uVP;
uniform mat4 uM;
out vec3 vNorm;
out vec3 vPos;
void main() {
    vec4 wPos = uM * vec4(aPos, 1.0);
    vPos = wPos.xyz;
    vNorm = mat3(uM) * aNorm;
    gl_Position = uVP * wPos;
})";

static const char* FS_CUBE = R"(#version 300 es
precision mediump float;
in vec3 vNorm;
in vec3 vPos;
uniform vec3 uCam;
out vec4 FragColor;
void main() {
    vec3 n = normalize(vNorm);
    vec3 v = normalize(uCam - vPos);
    vec3 light = normalize(vec3(0.6, 0.9, 0.5));
    float diff = max(dot(n, light), 0.0);
    float fresnel = pow(1.0 - max(dot(n, v), 0.0), 2.5) * 0.35;
    vec3 clay = vec3(0.56, 0.58, 0.65);
    vec3 col = clay * (diff + 0.25) + vec3(0.85, 0.9, 1.0) * fresnel;
    FragColor = vec4(col, 1.0);
})";

static const char* VS_LINE = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
uniform mat4 uVP;
out vec4 vCol;
void main() {
    vCol = aCol;
    gl_Position = uVP * vec4(aPos, 1.0);
})";

static const char* FS_LINE = R"(#version 300 es
precision mediump float;
in vec4 vCol;
out vec4 FragColor;
void main() { FragColor = vCol; })";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

bool Renderer::initEGL(ANativeWindow* window) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs);
    surface = eglCreateWindowSurface(display, config, window, nullptr);

    eglMakeCurrent(display, surface, surface, context);
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);

    initShaders();
    mesh.init();
    gizmo.init();
    return true;
}

void Renderer::initShaders() {
    GLuint vsC = compile(GL_VERTEX_SHADER, VS_CUBE);
    GLuint fsC = compile(GL_FRAGMENT_SHADER, FS_CUBE);
    cubeShader = glCreateProgram();
    glAttachShader(cubeShader, vsC);
    glAttachShader(cubeShader, fsC);
    glLinkProgram(cubeShader);

    GLuint vsL = compile(GL_VERTEX_SHADER, VS_LINE);
    GLuint fsL = compile(GL_FRAGMENT_SHADER, FS_LINE);
    lineShader = glCreateProgram();
    glAttachShader(lineShader, vsL);
    glAttachShader(lineShader, fsL);
    glLinkProgram(lineShader);
}

void Renderer::destroyEGL() {
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
}

void Renderer::renderFrame() {
    if (surface == EGL_NO_SURFACE) return;

    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    glViewport(0, 0, width, height);

    // لون خلفية بلندر الرمادية الأصلية
    glClearColor(0.18f, 0.19f, 0.22f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    Mat4 v = camera.getViewMatrix();
    Mat4 p = camera.getProjectionMatrix((float)width, (float)height);
    Mat4 vp = p * v;
    Mat4 m = Mat4::identity();
    Vec3 camPos = camera.getPosition();

    // 1. رسم شبكة الأرضية
    glUseProgram(lineShader);
    glUniformMatrix4fv(glGetUniformLocation(lineShader, "uVP"), 1, GL_FALSE, vp.m);
    mesh.renderGrid();

    // 2. رسم المكعب بشيدر بلندر
    glUseProgram(cubeShader);
    glUniformMatrix4fv(glGetUniformLocation(cubeShader, "uVP"), 1, GL_FALSE, vp.m);
    glUniformMatrix4fv(glGetUniformLocation(cubeShader, "uM"), 1, GL_FALSE, m.m);
    glUniform3f(glGetUniformLocation(cubeShader, "uCam"), camPos.x, camPos.y, camPos.z);
    mesh.renderCube();

    // 3. رسم الجزمو ثلاثي الأبعاد فوق كل شيء
    glUseProgram(lineShader);
    glUniformMatrix4fv(glGetUniformLocation(lineShader, "uVP"), 1, GL_FALSE, vp.m);
    gizmo.render(vp);

    eglSwapBuffers(display, surface);
}

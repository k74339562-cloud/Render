#include "gles_engine.h"
#include <android/log.h>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GLESEngine", __VA_ARGS__)

// شيدر المجسم المطور: يدعم الإضاءة + تلوين الوجه المحدد فورياً
static const char* MESH_VERT = R"(#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

uniform mat4 uViewProj;
uniform mat4 uModel;

out vec3 fragNormal;
out vec3 fragWorldPos;
out vec3 fragColor;

void main() {
    vec4 worldPos = uModel * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(uModel) * inNormal;
    fragColor = inColor;
    gl_Position = uViewProj * worldPos;
}
)";

static const char* MESH_FRAG = R"(#version 300 es
precision highp float;

in vec3 fragNormal;
in vec3 fragWorldPos;
in vec3 fragColor;

uniform vec3 uCamPos;
out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(uCamPos - fragWorldPos);

    vec3 camFwd = -V;
    vec3 camRight = normalize(cross(vec3(0.0, 0.0, 1.0), camFwd));
    if (length(camRight) < 0.001) camRight = vec3(1.0, 0.0, 0.0);
    vec3 camUp = cross(camFwd, camRight);

    // إضاءة استوديو بلندر الرباعية
    vec3 keyDir = normalize(camRight * 0.50 + camUp * 0.70 - camFwd * 0.50);
    vec3 keyCol = vec3(1.0, 0.98, 0.95);
    float diffKey = max(dot(N, keyDir), 0.0);

    vec3 fillDir = normalize(-camRight * 0.65 + camUp * 0.25 - camFwd * 0.40);
    vec3 fillCol = vec3(0.60, 0.72, 0.90);
    float diffFill = max(dot(N, fillDir), 0.0) * 0.45;

    vec3 rimDir = normalize(-camUp * 0.70 + camFwd * 0.65);
    vec3 rimCol = vec3(0.85, 0.92, 1.00);
    float diffRim = max(dot(N, rimDir), 0.0) * 0.35;

    vec3 bounceCol = vec3(0.35, 0.35, 0.37);
    float diffBounce = max(dot(N, vec3(0.0, 0.0, -1.0)), 0.0) * 0.22;

    vec3 H = normalize(keyDir + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.25;

    float NdotV = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.5) * 0.35;
    vec3 fresnelCol = vec3(0.95, 0.98, 1.0) * fresnel;

    vec3 ambient = vec3(0.20, 0.20, 0.22);
    vec3 totalLight = ambient + (keyCol * diffKey) + (fillCol * diffFill) + (rimCol * diffRim) + (bounceCol * diffBounce);
    vec3 linearColor = (fragColor * totalLight) + vec3(spec) + fresnelCol;

    outColor = vec4(pow(linearColor, vec3(1.0 / 2.2)), 1.0);
}
)";

static const char* LINE_VERT = R"(#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

uniform mat4 uViewProj;
uniform mat4 uModel;

out vec4 fragColor;

void main() {
    fragColor = inColor;
    gl_Position = uViewProj * uModel * vec4(inPosition, 1.0);
}
)";

static const char* LINE_FRAG = R"(#version 300 es
precision mediump float;
in vec4 fragColor;
out vec4 outColor;

void main() {
    outColor = fragColor;
}
)";

GLuint GLESEngine::compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(s, sizeof(info), nullptr, info);
        LOGE("Shader compile error: %s", info);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint GLESEngine::buildProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) return 0;

    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint linked;
    glGetProgramiv(p, GL_LINK_STATUS, &linked);
    if (!linked) {
        char info[512];
        glGetProgramInfoLog(p, sizeof(info), nullptr, info);
        LOGE("Program link error: %s", info);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

bool GLESEngine::init(ANativeWindow* window) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, window, nullptr);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("eglMakeCurrent failed!");
        return false;
    }

    width = ANativeWindow_getWidth(window);
    height = ANativeWindow_getHeight(window);

    meshProgram = buildProgram(MESH_VERT, MESH_FRAG);
    uMeshViewProj = glGetUniformLocation(meshProgram, "uViewProj");
    uMeshModel = glGetUniformLocation(meshProgram, "uModel");
    uMeshCamPos = glGetUniformLocation(meshProgram, "uCamPos");

    lineProgram = buildProgram(LINE_VERT, LINE_FRAG);
    uLineViewProj = glGetUniformLocation(lineProgram, "uViewProj");
    uLineModel = glGetUniformLocation(lineProgram, "uModel");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void GLESEngine::cleanup() {
    if (meshProgram) { glDeleteProgram(meshProgram); meshProgram = 0; }
    if (lineProgram) { glDeleteProgram(lineProgram); lineProgram = 0; }

    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
}

void GLESEngine::beginFrame(const Mat4& viewProj, const Vec3& camPos) {
    currentViewProj = viewProj;
    currentCamPos = camPos;

    glViewport(0, 0, width, height);
    glClearColor(0.204f, 0.204f, 0.204f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLESEngine::useMeshProgram(const Mat4& model) {
    glUseProgram(meshProgram);
    glUniformMatrix4fv(uMeshViewProj, 1, GL_FALSE, currentViewProj.m);
    glUniformMatrix4fv(uMeshModel, 1, GL_FALSE, model.m);
    glUniform3f(uMeshCamPos, currentCamPos.x, currentCamPos.y, currentCamPos.z);
}

void GLESEngine::useLineProgram(const Mat4& model) {
    glUseProgram(lineProgram);
    glUniformMatrix4fv(uLineViewProj, 1, GL_FALSE, currentViewProj.m);
    glUniformMatrix4fv(uLineModel, 1, GL_FALSE, model.m);
}

void GLESEngine::endFrame() {
    eglSwapBuffers(display, surface);
}

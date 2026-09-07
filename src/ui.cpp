#include "ui.h"
#include <cmath>
#include <algorithm>

struct UIVertex {
    float x, y;          // إحداثيات الشاشة بالبكسل
    float localX, localY;// الإحداثيات المحلية داخل الزر من المركز
    float w, h;          // عرض وارتفاع الزر
    float r, g, b, a;    // لون خلفية الزر
    float br, bg, bb, ba;// لون إطار الزر
    float iconType;      // نوع الأيقونة
};

// شيدر الـ SDF الإجرائي: يرسم أزرار منحنية الحواف وأيقونات بدون أي صور
static const char* UI_VERT = R"(#version 300 es
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inLocalPos;
layout(location = 2) in vec2 inSize;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inBorderColor;
layout(location = 5) in float inIconType;

uniform vec2 uScreenSize;

out vec2 vLocalPos;
out vec2 vSize;
out vec4 vColor;
out vec4 vBorderColor;
out float vIconType;

void main() {
    // تحويل بكسلات الشاشة المباشرة إلى فضاء NDC [-1, 1]
    vec2 ndc = (inPos / uScreenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y; // عكس اتجاه Y ليطابق نظام شاشات اللمس
    gl_Position = vec4(ndc, 0.0, 1.0);

    vLocalPos = inLocalPos;
    vSize = inSize;
    vColor = inColor;
    vBorderColor = inBorderColor;
    vIconType = inIconType;
}
)";

static const char* UI_FRAG = R"(#version 300 es
precision mediump float;

in vec2 vLocalPos;
in vec2 vSize;
in vec4 vColor;
in vec4 vBorderColor;
in float vIconType;

out vec4 outColor;

// دالة المسافة الرياضية للمستطيل المستدير (SDF)
float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 d = abs(p) - b + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

void main() {
    float radius = 12.0; // نعومة انحناء زوايا الزر
    vec2 halfSize = vSize * 0.5;
    float dist = roundedBoxSDF(vLocalPos, halfSize, radius);

    // نعومة الحواف الفائقة (Anti-Aliasing)
    float alpha = clamp(0.5 - dist, 0.0, 1.0);
    if (alpha <= 0.0) discard;

    // رسم الإطار الخارجي الناعم
    float borderThickness = 2.0;
    float borderFactor = clamp(dist + borderThickness, 0.0, 1.0);
    vec4 baseColor = mix(vColor, vBorderColor, borderFactor);

    // رسم الأيقونات الهندسية بذكاء
    vec4 finalColor = baseColor;
    vec2 p = vLocalPos;

    if (vIconType > 0.5 && vIconType < 1.5) {
        // أيقونة النقطة (Vertex Dot)
        float dotDist = length(p) - 5.0;
        float dotAlpha = clamp(0.5 - dotDist, 0.0, 1.0);
        finalColor = mix(finalColor, vec4(1.0), dotAlpha);
    } 
    else if (vIconType > 1.5 && vIconType < 2.5) {
        // أيقونة الحافة (Edge Line)
        float lineDist = abs(p.x + p.y * 0.5) - 2.0;
        float lineAlpha = clamp(0.5 - lineDist, 0.0, 1.0) * step(abs(p.x), 12.0);
        finalColor = mix(finalColor, vec4(1.0), lineAlpha);
    } 
    else if (vIconType > 2.5 && vIconType < 3.5) {
        // أيقونة الوجه (Face Quad)
        vec2 qd = abs(p) - vec2(8.0);
        float quadDist = min(max(qd.x, qd.y), 0.0) + length(max(qd, 0.0));
        float quadAlpha = clamp(0.5 - quadDist, 0.0, 1.0);
        finalColor = mix(finalColor, vec4(1.0), quadAlpha * 0.85);
    }

    outColor = vec4(finalColor.rgb, finalColor.a * alpha);
}
)";

void UIManager::init() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &UI_VERT, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &UI_FRAG, nullptr);
    glCompileShader(fs);

    uiProgram = glCreateProgram();
    glAttachShader(uiProgram, vs);
    glAttachShader(uiProgram, fs);
    glLinkProgram(uiProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    uScreenSize = glGetUniformLocation(uiProgram, "uScreenSize");

    glGenVertexArrays(1, &uiVao);
    glBindVertexArray(uiVao);
    glGenBuffers(1, &uiVbo);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);

    GLsizei stride = sizeof(UIVertex);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIVertex, x));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIVertex, localX));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIVertex, w));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIVertex, r));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIVertex, br));
    glEnableVertexAttribArray(5); glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(UIVertex, iconType));

    glBindVertexArray(0);
}

void UIManager::cleanup() {
    if (uiVao) { glDeleteVertexArrays(1, &uiVao); uiVao = 0; }
    if (uiVbo) { glDeleteBuffers(1, &uiVbo); uiVbo = 0; }
    if (uiProgram) { glDeleteProgram(uiProgram); uiProgram = 0; }
}

void UIManager::buildButtons(float screenW, float screenH, const MeshData& mesh) {
    buttons.clear();

    // مقاسات ديناميكية متوافقة مع حجم أصابع اليد
    float btnH = std::clamp(screenH * 0.07f, 50.0f, 75.0f);
    float topY = screenH * 0.04f;
    float startX = 25.0f;
    float gap = 12.0f;

    bool isEdit = (mesh.selectMode != SelectionMode::OBJECT);

    // 1. زر النمط الرئيسي (OBJECT vs EDIT)
    UIButton modeBtn;
    modeBtn.id = 1;
    modeBtn.x = startX;
    modeBtn.y = topY;
    modeBtn.w = std::clamp(screenW * 0.30f, 130.0f, 220.0f);
    modeBtn.h = btnH;

    if (isEdit) {
        modeBtn.r = 0.18f; modeBtn.g = 0.46f; modeBtn.b = 0.82f; modeBtn.a = 0.92f; // أزرق بلندر
        modeBtn.borderR = 0.40f; modeBtn.borderG = 0.70f; modeBtn.borderB = 1.0f; modeBtn.borderA = 1.0f;
    } else {
        modeBtn.r = 0.91f; modeBtn.g = 0.42f; modeBtn.b = 0.10f; modeBtn.a = 0.92f; // برتقالي بلندر
        modeBtn.borderR = 1.00f; modeBtn.borderG = 0.65f; modeBtn.borderB = 0.30f; modeBtn.borderA = 1.0f;
    }
    buttons.push_back(modeBtn);

    // 2. أزرار نمط التعديل الثلاثة (تظهر فقط عند تفعيل Edit Mode)
    if (isEdit) {
        float subBtnW = btnH * 1.15f;
        float currentX = startX + modeBtn.w + gap + 15.0f;

        // زر النقاط
        UIButton vBtn;
        vBtn.id = 2; vBtn.x = currentX; vBtn.y = topY; vBtn.w = subBtnW; vBtn.h = btnH;
        vBtn.icon = UIIcon::VERTEX_DOT;
        bool vSel = (mesh.selectMode == SelectionMode::VERTEX);
        vBtn.r = vSel ? 0.32f : 0.18f; vBtn.g = vSel ? 0.55f : 0.18f; vBtn.b = vSel ? 0.88f : 0.20f; vBtn.a = 0.90f;
        vBtn.borderR = vSel ? 1.0f : 0.35f; vBtn.borderG = vSel ? 0.7f : 0.35f; vBtn.borderB = 0.35f; vBtn.borderA = 1.0f;
        buttons.push_back(vBtn);

        // زر الحواف
        currentX += subBtnW + gap;
        UIButton eBtn;
        eBtn.id = 3; eBtn.x = currentX; eBtn.y = topY; eBtn.w = subBtnW; eBtn.h = btnH;
        eBtn.icon = UIIcon::EDGE_LINE;
        bool eSel = (mesh.selectMode == SelectionMode::EDGE);
        eBtn.r = eSel ? 0.32f : 0.18f; eBtn.g = eSel ? 0.55f : 0.18f; eBtn.b = eSel ? 0.88f : 0.20f; eBtn.a = 0.90f;
        eBtn.borderR = eSel ? 1.0f : 0.35f; eBtn.borderG = eSel ? 0.7f : 0.35f; eBtn.borderB = 0.35f; eBtn.borderA = 1.0f;
        buttons.push_back(eBtn);

        // زر الأوجه
        currentX += subBtnW + gap;
        UIButton fBtn;
        fBtn.id = 4; fBtn.x = currentX; fBtn.y = topY; fBtn.w = subBtnW; fBtn.h = btnH;
        fBtn.icon = UIIcon::FACE_QUAD;
        bool fSel = (mesh.selectMode == SelectionMode::FACE);
        fBtn.r = fSel ? 0.32f : 0.18f; fBtn.g = fSel ? 0.55f : 0.18f; fBtn.b = fSel ? 0.88f : 0.20f; fBtn.a = 0.90f;
        fBtn.borderR = fSel ? 1.0f : 0.35f; fBtn.borderG = fSel ? 0.7f : 0.35f; fBtn.borderB = 0.35f; fBtn.borderA = 1.0f;
        buttons.push_back(fBtn);
    }
}

bool UIManager::handleTouch(float touchX, float touchY, bool isDown, MeshData& mesh) {
    if (!isDown) return false;

    for (const auto& btn : buttons) {
        if (touchX >= btn.x && touchX <= btn.x + btn.w &&
            touchY >= btn.y && touchY <= btn.y + btn.h) {

            if (btn.id == 1) {
                // تبديل النمط
                if (mesh.selectMode == SelectionMode::OBJECT) {
                    mesh.selectMode = SelectionMode::VERTEX;
                    mesh.isObjectSelected = false;
                    mesh.selectedVertexIdx = 0;
                } else {
                    mesh.selectMode = SelectionMode::OBJECT;
                    mesh.isObjectSelected = true;
                }
            } else if (btn.id == 2) {
                mesh.selectMode = SelectionMode::VERTEX;
                mesh.selectedVertexIdx = 0;
            } else if (btn.id == 3) {
                mesh.selectMode = SelectionMode::EDGE;
                mesh.selectedEdgeIdx = 0;
            } else if (btn.id == 4) {
                mesh.selectMode = SelectionMode::FACE;
                mesh.selectedFaceIdx = 1;
            }

            mesh.rebuildBuffers();
            return true; // استهلاك اللمسة للزر فقط ومنع تدوير الكاميرا
        }
    }
    return false;
}

void UIManager::render(float screenW, float screenH, const MeshData& mesh) {
    buildButtons(screenW, screenH, mesh);
    if (buttons.empty()) return;

    std::vector<UIVertex> vertices;
    for (const auto& b : buttons) {
        float x0 = b.x, y0 = b.y;
        float x1 = b.x + b.w, y1 = b.y + b.h;
        float hw = b.w * 0.5f, hh = b.h * 0.5f;
        float it = (float)b.icon;

        // مثلثين لتكوين مستطيل الزر
        vertices.push_back({x0, y0, -hw, -hh, b.w, b.h, b.r, b.g, b.b, b.a, b.borderR, b.borderG, b.borderB, b.borderA, it});
        vertices.push_back({x1, y0,  hw, -hh, b.w, b.h, b.r, b.g, b.b, b.a, b.borderR, b.borderG, b.borderB, b.borderA, it});
        vertices.push_back({x1, y1,  hw,  hh, b.w, b.h, b.r, b.g, b.b, b.a, b.borderR, b.borderG, b.borderB, b.borderA, it});

        vertices.push_back({x0, y0, -hw, -hh, b.w, b.h, b.r, b.g, b.b, b.a, b.borderR, b.borderG, b.borderB, b.borderA, it});
        vertices.push_back({x1, y1,  hw,  hh, b.w, b.h, b.r, b.g, b.b, b.a, b.borderR, b.borderG, b.borderB, b.borderA, it});
        vertices.push_back({x0, y1, -hw,  hh, b.w, b.h, b.r, b.g, b.b, b.a, b.borderR, b.borderG, b.borderB, b.borderA, it});
    }

    glDisable(GL_DEPTH_TEST); // الواجهة ترسم دائماً فوق كل مجسمات الـ 3D
    glUseProgram(uiProgram);
    glUniform2f(uScreenSize, screenW, screenH);

    glBindVertexArray(uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(UIVertex), vertices.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

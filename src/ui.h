#pragma once
#include <GLES3/gl3.h>
#include <vector>
#include "mesh_data.h"

enum class UIIcon {
    NONE = 0,
    VERTEX_DOT = 1,
    EDGE_LINE = 2,
    FACE_QUAD = 3
};

struct UIButton {
    float x, y, w, h;
    float r, g, b, a;
    float borderR, borderG, borderB, borderA;
    UIIcon icon = UIIcon::NONE;
    bool isPressed = false;
    int id = 0;
};

class UIManager {
public:
    GLuint uiProgram = 0;
    GLuint uiVao = 0, uiVbo = 0;

    GLint uScreenSize = -1;

    void init();
    void cleanup();

    // فحص لمس الأزرار (يرجع true إذا لمس المستخدم أي زر لمنع دوران الكاميرا 3D)
    bool handleTouch(float touchX, float touchY, bool isDown, MeshData& mesh);

    // رسم واجهة المستخدم بالكامل في 1 Draw Call
    void render(float screenW, float screenH, const MeshData& mesh);

private:
    std::vector<UIButton> buttons;
    void buildButtons(float screenW, float screenH, const MeshData& mesh);
};

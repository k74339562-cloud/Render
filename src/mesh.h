#pragma once
#include <GLES3/gl3.h>

class Mesh {
public:
    GLuint cubeVao = 0, cubeVbo = 0, cubeIbo = 0;
    GLuint gridVao = 0, gridVbo = 0;
    int gridVertexCount = 0;

    void init();
    void renderCube();
    void renderGrid();
};

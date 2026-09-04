#pragma once
#include <GLES3/gl3.h>

class Mesh {
public:
    GLuint cubeVbo = 0, cubeIbo = 0;
    GLuint gridVbo = 0;
    int gridVertexCount = 0;

    void init();
    void renderCube();
    void renderGrid();
};

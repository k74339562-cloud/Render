#include "mesh.h"
#include <vector>

struct Vtx { float x, y, z, nx, ny, nz; };
struct LineVtx { float x, y, z, r, g, b, a; };

void Mesh::init() {
    Vtx cube[] = {
        {-1,-1, 1, 0,0,1}, { 1,-1, 1, 0,0,1}, { 1, 1, 1, 0,0,1}, {-1, 1, 1, 0,0,1},
        { 1,-1,-1, 0,0,-1}, {-1,-1,-1, 0,0,-1}, {-1, 1,-1, 0,0,-1}, { 1, 1,-1, 0,0,-1},
        {-1, 1, 1, 0,1,0}, { 1, 1, 1, 0,1,0}, { 1, 1,-1, 0,1,0}, {-1, 1,-1, 0,1,0},
        {-1,-1,-1, 0,-1,0}, { 1,-1,-1, 0,-1,0}, { 1,-1, 1, 0,-1,0}, {-1,-1, 1, 0,-1,0},
        { 1,-1, 1, 1,0,0}, { 1,-1,-1, 1,0,0}, { 1, 1,-1, 1,0,0}, { 1, 1, 1, 1,0,0},
        {-1,-1,-1, -1,0,0}, {-1,-1, 1, -1,0,0}, {-1, 1, 1, -1,0,0}, {-1, 1,-1, -1,0,0},
    };

    unsigned short idx[] = {
        0,1,2, 0,2,3,    4,5,6, 4,6,7,    8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
    };

    glGenBuffers(1, &cubeVbo);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    glGenBuffers(1, &cubeIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

    // شبكة أرضية بلندر
    std::vector<LineVtx> grid;
    int size = 10;
    for (int i = -size; i <= size; ++i) {
        float fi = (float)i, fs = (float)size;
        float r = 0.22f, g = 0.24f, b = 0.28f;
        if (i == 0) { r = 0.85f; g = 0.2f; b = 0.2f; } // محور X
        grid.push_back({-fs, -1.0f, fi, r, g, b, 1.0f});
        grid.push_back({ fs, -1.0f, fi, r, g, b, 1.0f});

        r = 0.22f; g = 0.24f; b = 0.28f;
        if (i == 0) { r = 0.2f; g = 0.8f; b = 0.25f; } // محور Z
        grid.push_back({fi, -1.0f, -fs, r, g, b, 1.0f});
        grid.push_back({fi, -1.0f,  fs, r, g, b, 1.0f});
    }

    gridVertexCount = (int)grid.size();
    glGenBuffers(1, &gridVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
    glBufferData(GL_ARRAY_BUFFER, grid.size() * sizeof(LineVtx), grid.data(), GL_STATIC_DRAW);
}

void Mesh::renderCube() {
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)(3 * sizeof(float)));
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
}

void Mesh::renderGrid() {
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVtx), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVtx), (void*)(3 * sizeof(float)));
    glDrawArrays(GL_LINES, 0, gridVertexCount);
}

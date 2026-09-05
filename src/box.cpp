#include "box.h"
#include <cstring>

struct VertexCube { float x, y, z; float nx, ny, nz; };
struct VertexLine { float x, y, z; float r, g, b, a; };

void Box::init(RenderEngine& engine) {
    VertexCube cubeVertices[] = {
        {-1,-1, 1,  0,0,1}, { 1,-1, 1,  0,0,1}, { 1, 1, 1,  0,0,1}, {-1, 1, 1,  0,0,1},
        { 1,-1,-1,  0,0,-1},{-1,-1,-1,  0,0,-1},{-1, 1,-1,  0,0,-1},{ 1, 1,-1,  0,0,-1},
        {-1, 1, 1,  0,1,0}, { 1, 1, 1,  0,1,0}, { 1, 1,-1,  0,1,0}, {-1, 1,-1,  0,1,0},
        {-1,-1,-1,  0,-1,0},{ 1,-1,-1,  0,-1,0},{ 1,-1, 1,  0,-1,0},{-1,-1, 1,  0,-1,0},
        { 1,-1, 1,  1,0,0}, { 1,-1,-1,  1,0,0}, { 1, 1,-1,  1,0,0}, { 1, 1, 1,  1,0,0},
        {-1,-1,-1, -1,0,0}, {-1,-1, 1, -1,0,0}, {-1, 1, 1, -1,0,0}, {-1, 1,-1, -1,0,0}
    };
    uint16_t cubeIndices[] = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
    };

    engine.createBuffer(sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vbo, vboMemory);
    void* vData; vkMapMemory(engine.device, vboMemory, 0, sizeof(cubeVertices), 0, &vData);
    memcpy(vData, cubeVertices, sizeof(cubeVertices)); vkUnmapMemory(engine.device, vboMemory);

    engine.createBuffer(sizeof(cubeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ibo, iboMemory);
    void* iData; vkMapMemory(engine.device, iboMemory, 0, sizeof(cubeIndices), 0, &iData);
    memcpy(iData, cubeIndices, sizeof(cubeIndices)); vkUnmapMemory(engine.device, iboMemory);

    // حواف تحديد بلندر البرتقالية (#E86C19)
    const float er = 0.91f, eg = 0.42f, eb = 0.10f;
    VertexLine cubeEdges[] = {
        {-1,-1,-1, er,eg,eb,1.0f}, { 1,-1,-1, er,eg,eb,1.0f},
        { 1,-1,-1, er,eg,eb,1.0f}, { 1, 1,-1, er,eg,eb,1.0f},
        { 1, 1,-1, er,eg,eb,1.0f}, {-1, 1,-1, er,eg,eb,1.0f},
        {-1, 1,-1, er,eg,eb,1.0f}, {-1,-1,-1, er,eg,eb,1.0f},
        {-1,-1, 1, er,eg,eb,1.0f}, { 1,-1, 1, er,eg,eb,1.0f},
        { 1,-1, 1, er,eg,eb,1.0f}, { 1, 1, 1, er,eg,eb,1.0f},
        { 1, 1, 1, er,eg,eb,1.0f}, {-1, 1, 1, er,eg,eb,1.0f},
        {-1, 1, 1, er,eg,eb,1.0f}, {-1,-1, 1, er,eg,eb,1.0f},
        {-1,-1,-1, er,eg,eb,1.0f}, {-1,-1, 1, er,eg,eb,1.0f},
        { 1,-1,-1, er,eg,eb,1.0f}, { 1,-1, 1, er,eg,eb,1.0f},
        { 1, 1,-1, er,eg,eb,1.0f}, { 1, 1, 1, er,eg,eb,1.0f},
        {-1, 1,-1, er,eg,eb,1.0f}, {-1, 1, 1, er,eg,eb,1.0f}
    };
    engine.createBuffer(sizeof(cubeEdges), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, edgesVbo, edgesVboMemory);
    void* edgeData; vkMapMemory(engine.device, edgesVboMemory, 0, sizeof(cubeEdges), 0, &edgeData);
    memcpy(edgeData, cubeEdges, sizeof(cubeEdges)); vkUnmapMemory(engine.device, edgesVboMemory);
}

void Box::cleanup(RenderEngine& engine) {
    engine.destroyBuffer(edgesVbo, edgesVboMemory);
    engine.destroyBuffer(ibo, iboMemory);
    engine.destroyBuffer(vbo, vboMemory);
    vbo = ibo = edgesVbo = VK_NULL_HANDLE;
}

void Box::draw(RenderEngine& engine) {
    Mat4 model = getModelMatrix();
    engine.drawMesh(vbo, ibo, 36, model);
    engine.drawOverlayLines(edgesVbo, 24, model);
}

Mat4 Box::getModelMatrix() const {
    return Mat4::translate(position);
}

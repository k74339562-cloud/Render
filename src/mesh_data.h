#pragma once
#include <vector>
#include <GLES3/gl3.h>
#include "math_3d.h"
#include "camera.h"

class GLESEngine;

enum class SelectionMode {
    OBJECT = 0,
    VERTEX = 1,
    EDGE   = 2,
    FACE   = 3
};

struct MeshVertex {
    Vec3 pos;
    Vec3 normal;
    bool selected = false;
};

struct MeshEdge {
    uint32_t v0;
    uint32_t v1;
    bool selected = false;
};

struct MeshFace {
    uint32_t v[4];
    Vec3 normal;
    Vec3 center;
    bool selected = false;
};

class MeshData {
public:
    Vec3 position = {0.0f, 0.0f, 0.0f};
    bool isObjectSelected = true;
    SelectionMode selectMode = SelectionMode::OBJECT;

    int selectedVertexIdx = -1;
    int selectedEdgeIdx = -1;
    int selectedFaceIdx = -1;

    std::vector<MeshVertex> vertices;
    std::vector<MeshEdge> edges;
    std::vector<MeshFace> faces;

    GLuint faceVao = 0, faceVbo = 0, faceIbo = 0;
    uint32_t faceIndexCount = 0;

    GLuint edgeVao = 0, edgeVbo = 0;
    uint32_t edgeVertexCount = 0;

    GLuint vertDotsVao = 0, vertDotsVbo = 0;
    uint32_t vertDotsCount = 0;

    void initDefaultCube();
    void cleanup();
    void rebuildBuffers();

    void deselectAll();

    // دوال التحديد الحقيقي المعتمدة على بكسلات الشاشة وحجب الأجزاء الخلفية
    bool pickObject(const Ray& ray, float& outDist);
    int pickFace(const Ray& ray, const Camera& camera);
    int pickVertex(float touchX, float touchY, float screenW, float screenH, const Camera& camera, float maxPixelDist = 55.0f);
    int pickEdge(float touchX, float touchY, float screenW, float screenH, const Camera& camera, float maxPixelDist = 45.0f);

    Vec3 getActiveGizmoPosition() const;
    Mat4 getActiveGizmoOrientation() const;

    Mat4 getModelMatrix() const;
    void draw(GLESEngine& engine);
};

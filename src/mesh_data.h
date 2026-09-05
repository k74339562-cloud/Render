#pragma once
#include <vector>
#include "math_3d.h"
#include "render_engine.h"

enum class SelectionMode {
    OBJECT = 0, // تحديد كامل المكعب
    VERTEX = 1, // تحديد النقاط
    EDGE   = 2, // تحديد الحواف
    FACE   = 3  // تحديد الأوجه
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
    uint32_t v[4]; // أوجه رباعية Quad كبلندر
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

    // بافرات كرت الشاشة
    VkBuffer faceVbo = VK_NULL_HANDLE;
    VkDeviceMemory faceVboMemory = VK_NULL_HANDLE;
    VkBuffer faceIbo = VK_NULL_HANDLE;
    VkDeviceMemory faceIboMemory = VK_NULL_HANDLE;

    VkBuffer edgeVbo = VK_NULL_HANDLE;
    VkDeviceMemory edgeVboMemory = VK_NULL_HANDLE;
    uint32_t edgeVertexCount = 0;

    VkBuffer vertDotsVbo = VK_NULL_HANDLE;
    VkDeviceMemory vertDotsVboMemory = VK_NULL_HANDLE;
    uint32_t vertDotsCount = 0;

    void initDefaultCube(RenderEngine& engine);
    void cleanup(RenderEngine& engine);
    void rebuildBuffers(RenderEngine& engine);

    void deselectAll();
    bool pickObject(const Ray& ray, float& outDist);
    int pickFace(const Ray& ray, float& outDist);
    int pickEdge(const Ray& ray, float threshold);
    int pickVertex(const Ray& ray, float threshold);

    Vec3 getActiveGizmoPosition() const;
    Mat4 getActiveGizmoOrientation() const; // حساب التوجيه الخارجي دائماً

    Mat4 getModelMatrix() const;
    void draw(RenderEngine& engine);
};

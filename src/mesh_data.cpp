#include "mesh_data.h"
#include "camera.h"
#include <cstring>
#include <cmath>
#include <algorithm>

struct VtxFace { float x, y, z; float nx, ny, nz; };
struct VtxLine { float x, y, z; float r, g, b, a; };

static bool intersectRayTriangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2, float& outT) {
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    Vec3 p = ray.dir.cross(e2);
    float det = e1.dot(p);

    if (std::abs(det) < 0.00001f) return false;
    float invDet = 1.0f / det;

    Vec3 t = ray.origin - v0;
    float u = t.dot(p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    Vec3 q = t.cross(e1);
    float v = ray.dir.dot(q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    float dist = e2.dot(q) * invDet;
    if (dist > 0.0001f) {
        outT = dist;
        return true;
    }
    return false;
}

static float distToSegment2D(float px, float py, float x0, float y0, float x1, float y1) {
    float vx = x1 - x0, vy = y1 - y0;
    float wx = px - x0, wy = py - y0;
    float c1 = wx * vx + wy * vy;
    if (c1 <= 0.0f) return std::sqrt(wx * wx + wy * wy);
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        float dx = px - x1, dy = py - y1;
        return std::sqrt(dx * dx + dy * dy);
    }
    float b = c1 / c2;
    float qx = x0 + b * vx, qy = y0 + b * vy;
    float dx = px - qx, dy = py - qy;
    return std::sqrt(dx * dx + dy * dy);
}

void MeshData::initDefaultCube(RenderEngine& engine) {
    vertices = {
        {{-1, -1, -1}, {-0.577f, -0.577f, -0.577f}}, // 0
        {{ 1, -1, -1}, { 0.577f, -0.577f, -0.577f}}, // 1
        {{ 1,  1, -1}, { 0.577f,  0.577f, -0.577f}}, // 2
        {{-1,  1, -1}, {-0.577f,  0.577f, -0.577f}}, // 3
        {{-1, -1,  1}, {-0.577f, -0.577f,  0.577f}}, // 4
        {{ 1, -1,  1}, { 0.577f, -0.577f,  0.577f}}, // 5
        {{ 1,  1,  1}, { 0.577f,  0.577f,  0.577f}}, // 6
        {{-1,  1,  1}, {-0.577f,  0.577f,  0.577f}}  // 7
    };

    edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    faces = {
        {{0, 3, 2, 1}, { 0,  0, -1}, { 0,  0, -1}}, // 0. أسفل (-Z)
        {{4, 5, 6, 7}, { 0,  0,  1}, { 0,  0,  1}}, // 1. أعلى (+Z)
        {{0, 1, 5, 4}, { 0, -1,  0}, { 0, -1,  0}}, // 2. أمام (-Y)
        {{2, 3, 7, 6}, { 0,  1,  0}, { 0,  1,  0}}, // 3. خلف (+Y)
        {{3, 0, 4, 7}, {-1,  0,  0}, {-1,  0,  0}}, // 4. يسار (-X)
        {{1, 2, 6, 5}, { 1,  0,  0}, { 1,  0,  0}}  // 5. يمين (+X)
    };

    rebuildBuffers(engine);
}

void MeshData::rebuildBuffers(RenderEngine& engine) {
    cleanup(engine);

    // 1. بافر الأوجه
    std::vector<VtxFace> faceVtx;
    std::vector<uint16_t> faceIdx;

    for (const auto& f : faces) {
        uint16_t startIdx = (uint16_t)faceVtx.size();
        for (int i = 0; i < 4; ++i) {
            faceVtx.push_back({vertices[f.v[i]].pos.x, vertices[f.v[i]].pos.y, vertices[f.v[i]].pos.z, f.normal.x, f.normal.y, f.normal.z});
        }
        faceIdx.push_back(startIdx + 0); faceIdx.push_back(startIdx + 1); faceIdx.push_back(startIdx + 2);
        faceIdx.push_back(startIdx + 0); faceIdx.push_back(startIdx + 2); faceIdx.push_back(startIdx + 3);
    }

    engine.createBuffer(faceVtx.size() * sizeof(VtxFace), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, faceVbo, faceVboMemory);
    void* vData; vkMapMemory(engine.device, faceVboMemory, 0, faceVtx.size() * sizeof(VtxFace), 0, &vData);
    memcpy(vData, faceVtx.data(), faceVtx.size() * sizeof(VtxFace)); vkUnmapMemory(engine.device, faceVboMemory);

    engine.createBuffer(faceIdx.size() * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, faceIbo, faceIboMemory);
    void* iData; vkMapMemory(engine.device, faceIboMemory, 0, faceIdx.size() * sizeof(uint16_t), 0, &iData);
    memcpy(iData, faceIdx.data(), faceIdx.size() * sizeof(uint16_t)); vkUnmapMemory(engine.device, faceIboMemory);

    // 2. تلوين الوجه المحدد كبلندر
    if (selectMode == SelectionMode::FACE && selectedFaceIdx >= 0 && selectedFaceIdx < (int)faces.size()) {
        const auto& sf = faces[selectedFaceIdx];
        std::vector<VtxLine> selFaceVtx;
        std::vector<uint16_t> selFaceIdx;

        Vec3 offset = sf.normal * 0.005f;
        for (int i = 0; i < 4; ++i) {
            Vec3 p = vertices[sf.v[i]].pos + offset;
            selFaceVtx.push_back({p.x, p.y, p.z, 0.98f, 0.65f, 0.12f, 0.35f});
        }
        selFaceIdx = {0, 1, 2, 0, 2, 3};

        engine.createBuffer(selFaceVtx.size() * sizeof(VtxLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, selFaceVbo, selFaceVboMemory);
        void* sData; vkMapMemory(engine.device, selFaceVboMemory, 0, selFaceVtx.size() * sizeof(VtxLine), 0, &sData);
        memcpy(sData, selFaceVtx.data(), selFaceVtx.size() * sizeof(VtxLine)); vkUnmapMemory(engine.device, selFaceVboMemory);

        engine.createBuffer(selFaceIdx.size() * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, selFaceIbo, selFaceIboMemory);
        void* siData; vkMapMemory(engine.device, selFaceIboMemory, 0, selFaceIdx.size() * sizeof(uint16_t), 0, &siData);
        memcpy(siData, selFaceIdx.data(), selFaceIdx.size() * sizeof(uint16_t)); vkUnmapMemory(engine.device, selFaceIboMemory);
    }

    // 3. بافر الحواف
    std::vector<VtxLine> edgeLines;
    for (size_t i = 0; i < edges.size(); ++i) {
        float r = 0.10f, g = 0.10f, b = 0.10f, a = 1.0f;

        if (isObjectSelected && selectMode == SelectionMode::OBJECT) {
            r = 0.91f; g = 0.42f; b = 0.10f;
        } else if (selectMode == SelectionMode::EDGE && (int)i == selectedEdgeIdx) {
            r = 1.00f; g = 0.70f; b = 0.00f;
        } else if (selectMode == SelectionMode::FACE && selectedFaceIdx >= 0) {
            const auto& sf = faces[selectedFaceIdx];
            for (int k = 0; k < 4; ++k) {
                uint32_t aIdx = sf.v[k];
                uint32_t bIdx = sf.v[(k + 1) % 4];
                if ((edges[i].v0 == aIdx && edges[i].v1 == bIdx) || (edges[i].v0 == bIdx && edges[i].v1 == aIdx)) {
                    r = 0.98f; g = 0.65f; b = 0.12f;
                    break;
                }
            }
        }

        const auto& p0 = vertices[edges[i].v0].pos;
        const auto& p1 = vertices[edges[i].v1].pos;
        edgeLines.push_back({p0.x, p0.y, p0.z, r, g, b, a});
        edgeLines.push_back({p1.x, p1.y, p1.z, r, g, b, a});
    }
    edgeVertexCount = (uint32_t)edgeLines.size();

    engine.createBuffer(edgeLines.size() * sizeof(VtxLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, edgeVbo, edgeVboMemory);
    void* eData; vkMapMemory(engine.device, edgeVboMemory, 0, edgeLines.size() * sizeof(VtxLine), 0, &eData);
    memcpy(eData, edgeLines.data(), edgeLines.size() * sizeof(VtxLine)); vkUnmapMemory(engine.device, edgeVboMemory);

    // 4. بافر نقاط الرؤوس
    std::vector<VtxLine> dots;
    if (selectMode == SelectionMode::VERTEX) {
        float sz = 0.045f;
        for (size_t i = 0; i < vertices.size(); ++i) {
            float r = 0.15f, g = 0.15f, b = 0.15f;
            if ((int)i == selectedVertexIdx) {
                r = 1.00f; g = 0.65f; b = 0.00f;
            }
            Vec3 p = vertices[i].pos;
            dots.push_back({p.x - sz, p.y, p.z, r, g, b, 1.0f}); dots.push_back({p.x + sz, p.y, p.z, r, g, b, 1.0f});
            dots.push_back({p.x, p.y - sz, p.z, r, g, b, 1.0f}); dots.push_back({p.x, p.y + sz, p.z, r, g, b, 1.0f});
            dots.push_back({p.x, p.y, p.z - sz, r, g, b, 1.0f}); dots.push_back({p.x, p.y, p.z + sz, r, g, b, 1.0f});
        }
    }
    vertDotsCount = (uint32_t)dots.size();
    if (vertDotsCount > 0) {
        engine.createBuffer(dots.size() * sizeof(VtxLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertDotsVbo, vertDotsVboMemory);
        void* dData; vkMapMemory(engine.device, vertDotsVboMemory, 0, dots.size() * sizeof(VtxLine), 0, &dData);
        memcpy(dData, dots.data(), dots.size() * sizeof(VtxLine)); vkUnmapMemory(engine.device, vertDotsVboMemory);
    }
}

void MeshData::cleanup(RenderEngine& engine) {
    if (selFaceVbo) { engine.destroyBuffer(selFaceVbo, selFaceVboMemory); selFaceVbo = VK_NULL_HANDLE; }
    if (selFaceIbo) { engine.destroyBuffer(selFaceIbo, selFaceIboMemory); selFaceIbo = VK_NULL_HANDLE; }
    if (vertDotsVbo) { engine.destroyBuffer(vertDotsVbo, vertDotsVboMemory); vertDotsVbo = VK_NULL_HANDLE; }
    if (edgeVbo) { engine.destroyBuffer(edgeVbo, edgeVboMemory); edgeVbo = VK_NULL_HANDLE; }
    if (faceIbo) { engine.destroyBuffer(faceIbo, faceIboMemory); faceIbo = VK_NULL_HANDLE; }
    if (faceVbo) { engine.destroyBuffer(faceVbo, faceVboMemory); faceVbo = VK_NULL_HANDLE; }
}

void MeshData::deselectAll() {
    isObjectSelected = false;
    selectedVertexIdx = -1;
    selectedEdgeIdx = -1;
    selectedFaceIdx = -1;
}

bool MeshData::pickObject(const Ray& ray, float& outDist) {
    Mat4 invModel = getModelMatrix().inverse();
    Ray localRay;
    localRay.origin = invModel.transformPoint(ray.origin);
    Vec3 target = invModel.transformPoint(ray.origin + ray.dir);
    localRay.dir = (target - localRay.origin).normalize();

    float closestT = 1e9f;
    bool hit = false;
    for (const auto& f : faces) {
        float t = 0.0f;
        if (intersectRayTriangle(localRay, vertices[f.v[0]].pos, vertices[f.v[1]].pos, vertices[f.v[2]].pos, t) ||
            intersectRayTriangle(localRay, vertices[f.v[0]].pos, vertices[f.v[2]].pos, vertices[f.v[3]].pos, t)) {
            if (t < closestT) { closestT = t; hit = true; }
        }
    }
    if (hit) outDist = closestT;
    return hit;
}

int MeshData::pickFace(const Ray& ray, float& outDist) {
    Mat4 invModel = getModelMatrix().inverse();
    Ray localRay;
    localRay.origin = invModel.transformPoint(ray.origin);
    Vec3 target = invModel.transformPoint(ray.origin + ray.dir);
    localRay.dir = (target - localRay.origin).normalize();

    float closestT = 1e9f;
    int bestIdx = -1;
    for (size_t i = 0; i < faces.size(); ++i) {
        float t = 0.0f;
        if (intersectRayTriangle(localRay, vertices[faces[i].v[0]].pos, vertices[faces[i].v[1]].pos, vertices[faces[i].v[2]].pos, t) ||
            intersectRayTriangle(localRay, vertices[faces[i].v[0]].pos, vertices[faces[i].v[2]].pos, vertices[faces[i].v[3]].pos, t)) {
            if (t < closestT) { closestT = t; bestIdx = (int)i; }
        }
    }
    if (bestIdx != -1) outDist = closestT;
    return bestIdx;
}

int MeshData::pickVertexScreen(const Camera& camera, float touchX, float touchY, float screenW, float screenH, float thresholdPx) {
    int bestIdx = -1;
    float minDist = thresholdPx;

    for (size_t i = 0; i < vertices.size(); ++i) {
        Vec3 worldP = position + vertices[i].pos;
        Vec2 screenP = camera.projectToScreen(worldP, screenW, screenH);
        float d = std::sqrt((touchX - screenP.x) * (touchX - screenP.x) + (touchY - screenP.y) * (touchY - screenP.y));
        if (d < minDist) {
            minDist = d;
            bestIdx = (int)i;
        }
    }
    return bestIdx;
}

int MeshData::pickEdgeScreen(const Camera& camera, float touchX, float touchY, float screenW, float screenH, float thresholdPx) {
    int bestIdx = -1;
    float minDist = thresholdPx;

    for (size_t i = 0; i < edges.size(); ++i) {
        Vec3 p0 = position + vertices[edges[i].v0].pos;
        Vec3 p1 = position + vertices[edges[i].v1].pos;
        Vec2 s0 = camera.projectToScreen(p0, screenW, screenH);
        Vec2 s1 = camera.projectToScreen(p1, screenW, screenH);

        float d = distToSegment2D(touchX, touchY, s0.x, s0.y, s1.x, s1.y);
        if (d < minDist) {
            minDist = d;
            bestIdx = (int)i;
        }
    }
    return bestIdx;
}

Vec3 MeshData::getActiveGizmoPosition() const {
    if (selectMode == SelectionMode::OBJECT || !isObjectSelected) {
        return position;
    }
    if (selectMode == SelectionMode::VERTEX && selectedVertexIdx >= 0) {
        return position + vertices[selectedVertexIdx].pos;
    }
    if (selectMode == SelectionMode::EDGE && selectedEdgeIdx >= 0) {
        Vec3 mid = (vertices[edges[selectedEdgeIdx].v0].pos + vertices[edges[selectedEdgeIdx].v1].pos) * 0.5f;
        return position + mid;
    }
    if (selectMode == SelectionMode::FACE && selectedFaceIdx >= 0) {
        return position + faces[selectedFaceIdx].center;
    }
    return position;
}

Mat4 MeshData::getActiveGizmoOrientation() const {
    Mat4 r = Mat4::identity();
    Vec3 gPos = getActiveGizmoPosition();
    r.m[12] = gPos.x;
    r.m[13] = gPos.y;
    r.m[14] = gPos.z;
    return r;
}

Mat4 MeshData::getModelMatrix() const {
    return Mat4::translate(position);
}

void MeshData::draw(RenderEngine& engine) {
    Mat4 model = getModelMatrix();
    engine.drawMesh(faceVbo, faceIbo, 36, model);

    if (selFaceVbo && selFaceIbo) {
        engine.drawGizmo(selFaceVbo, 6, model);
    }

    if (edgeVbo && edgeVertexCount > 0) {
        engine.drawOverlayLines(edgeVbo, edgeVertexCount, model);
    }

    if (vertDotsVbo && vertDotsCount > 0) {
        engine.drawOverlayLines(vertDotsVbo, vertDotsCount, model);
    }
}

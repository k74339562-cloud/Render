#include "mesh_data.h"
#include "gles_engine.h"
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

static float distRayToPoint(const Ray& ray, const Vec3& p) {
    Vec3 w = p - ray.origin;
    float c1 = w.dot(ray.dir);
    if (c1 <= 0.0f) return (p - ray.origin).length();
    Vec3 proj = ray.origin + ray.dir * c1;
    return (p - proj).length();
}

static float distRayToSegment3D(const Ray& ray, const Vec3& p0, const Vec3& p1) {
    Vec3 u = ray.dir;
    Vec3 v = p1 - p0;
    Vec3 w0 = ray.origin - p0;

    float a = u.dot(u), b = u.dot(v), c = v.dot(v), d = u.dot(w0), e = v.dot(w0);
    float denom = a * c - b * b;
    if (std::abs(denom) < 0.0001f) return (ray.origin - p0).length();

    float sc = std::max(0.0f, (b * e - c * d) / denom);
    float tc = std::clamp((a * e - b * d) / denom, 0.0f, 1.0f);

    Vec3 closestRay = ray.origin + u * sc;
    Vec3 closestSeg = p0 + v * tc;
    return (closestRay - closestSeg).length();
}

void MeshData::initDefaultCube() {
    vertices = {
        {{-1, -1, -1}, {-0.577f, -0.577f, -0.577f}},
        {{ 1, -1, -1}, { 0.577f, -0.577f, -0.577f}},
        {{ 1,  1, -1}, { 0.577f,  0.577f, -0.577f}},
        {{-1,  1, -1}, {-0.577f,  0.577f, -0.577f}},
        {{-1, -1,  1}, {-0.577f, -0.577f,  0.577f}},
        {{ 1, -1,  1}, { 0.577f, -0.577f,  0.577f}},
        {{ 1,  1,  1}, { 0.577f,  0.577f,  0.577f}},
        {{-1,  1,  1}, {-0.577f,  0.577f,  0.577f}}
    };

    edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    faces = {
        {{0, 3, 2, 1}, { 0,  0, -1}, { 0,  0, -1}},
        {{4, 5, 6, 7}, { 0,  0,  1}, { 0,  0,  1}},
        {{0, 1, 5, 4}, { 0, -1,  0}, { 0, -1,  0}},
        {{2, 3, 7, 6}, { 0,  1,  0}, { 0,  1,  0}},
        {{3, 0, 4, 7}, {-1,  0,  0}, {-1,  0,  0}},
        {{1, 2, 6, 5}, { 1,  0,  0}, { 1,  0,  0}}
    };

    rebuildBuffers();
}

void MeshData::rebuildBuffers() {
    cleanup();

    // 1. أوجه المجسم المصمتة
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
    faceIndexCount = (uint32_t)faceIdx.size();

    glGenVertexArrays(1, &faceVao);
    glBindVertexArray(faceVao);

    glGenBuffers(1, &faceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, faceVbo);
    glBufferData(GL_ARRAY_BUFFER, faceVtx.size() * sizeof(VtxFace), faceVtx.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &faceIbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, faceIdx.size() * sizeof(uint16_t), faceIdx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VtxFace), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VtxFace), (void*)(3 * sizeof(float)));

    // 2. خطوط الحواف
    std::vector<VtxLine> edgeLines;
    for (size_t i = 0; i < edges.size(); ++i) {
        float r = 0.10f, g = 0.10f, b = 0.10f, a = 1.0f;
        if (isObjectSelected && selectMode == SelectionMode::OBJECT) {
            r = 0.91f; g = 0.42f; b = 0.10f; // برتقالي بلندر
        } else if (selectMode == SelectionMode::EDGE && (int)i == selectedEdgeIdx) {
            r = 1.00f; g = 0.70f; b = 0.00f; // أصفر التحديد
        }

        const auto& p0 = vertices[edges[i].v0].pos;
        const auto& p1 = vertices[edges[i].v1].pos;
        edgeLines.push_back({p0.x, p0.y, p0.z, r, g, b, a});
        edgeLines.push_back({p1.x, p1.y, p1.z, r, g, b, a});
    }
    edgeVertexCount = (uint32_t)edgeLines.size();

    glGenVertexArrays(1, &edgeVao);
    glBindVertexArray(edgeVao);

    glGenBuffers(1, &edgeVbo);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVbo);
    glBufferData(GL_ARRAY_BUFFER, edgeLines.size() * sizeof(VtxLine), edgeLines.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VtxLine), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VtxLine), (void*)(3 * sizeof(float)));

    // 3. نقاط الرؤوس (Vertices)
    std::vector<VtxLine> dots;
    if (selectMode == SelectionMode::VERTEX) {
        float sz = 0.040f;
        for (size_t i = 0; i < vertices.size(); ++i) {
            float r = 0.18f, g = 0.18f, b = 0.18f;
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
        glGenVertexArrays(1, &vertDotsVao);
        glBindVertexArray(vertDotsVao);

        glGenBuffers(1, &vertDotsVbo);
        glBindBuffer(GL_ARRAY_BUFFER, vertDotsVbo);
        glBufferData(GL_ARRAY_BUFFER, dots.size() * sizeof(VtxLine), dots.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VtxLine), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VtxLine), (void*)(3 * sizeof(float)));
    }

    glBindVertexArray(0);
}

void MeshData::cleanup() {
    if (faceVao) { glDeleteVertexArrays(1, &faceVao); faceVao = 0; }
    if (faceVbo) { glDeleteBuffers(1, &faceVbo); faceVbo = 0; }
    if (faceIbo) { glDeleteBuffers(1, &faceIbo); faceIbo = 0; }

    if (edgeVao) { glDeleteVertexArrays(1, &edgeVao); edgeVao = 0; }
    if (edgeVbo) { glDeleteBuffers(1, &edgeVbo); edgeVbo = 0; }

    if (vertDotsVao) { glDeleteVertexArrays(1, &vertDotsVao); vertDotsVao = 0; }
    if (vertDotsVbo) { glDeleteBuffers(1, &vertDotsVbo); vertDotsVbo = 0; }
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

int MeshData::pickVertex(const Ray& ray, float threshold) {
    Mat4 invModel = getModelMatrix().inverse();
    Ray localRay;
    localRay.origin = invModel.transformPoint(ray.origin);
    Vec3 target = invModel.transformPoint(ray.origin + ray.dir);
    localRay.dir = (target - localRay.origin).normalize();

    int bestIdx = -1;
    float minDist = threshold;
    for (size_t i = 0; i < vertices.size(); ++i) {
        float d = distRayToPoint(localRay, vertices[i].pos);
        if (d < minDist) { minDist = d; bestIdx = (int)i; }
    }
    return bestIdx;
}

int MeshData::pickEdge(const Ray& ray, float threshold) {
    Mat4 invModel = getModelMatrix().inverse();
    Ray localRay;
    localRay.origin = invModel.transformPoint(ray.origin);
    Vec3 target = invModel.transformPoint(ray.origin + ray.dir);
    localRay.dir = (target - localRay.origin).normalize();

    int bestIdx = -1;
    float minDist = threshold;
    for (size_t i = 0; i < edges.size(); ++i) {
        float d = distRayToSegment3D(localRay, vertices[edges[i].v0].pos, vertices[edges[i].v1].pos);
        if (d < minDist) { minDist = d; bestIdx = (int)i; }
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
    Vec3 outZ = {0.0f, 0.0f, 1.0f};

    if (selectMode == SelectionMode::FACE && selectedFaceIdx >= 0) {
        outZ = faces[selectedFaceIdx].normal;
    } else if (selectMode == SelectionMode::VERTEX && selectedVertexIdx >= 0) {
        outZ = vertices[selectedVertexIdx].pos.normalize();
    } else if (selectMode == SelectionMode::EDGE && selectedEdgeIdx >= 0) {
        Vec3 mid = (vertices[edges[selectedEdgeIdx].v0].pos + vertices[edges[selectedEdgeIdx].v1].pos) * 0.5f;
        outZ = mid.normalize();
    }

    if (outZ.length() < 0.001f) outZ = Vec3(0, 0, 1);

    Vec3 helper = (std::abs(outZ.z) < 0.9f) ? Vec3(0, 0, 1) : Vec3(1, 0, 0);
    Vec3 outX = helper.cross(outZ).normalize();
    Vec3 outY = outZ.cross(outX).normalize();

    Mat4 r = Mat4::identity();
    r.m[0] = outX.x; r.m[1] = outX.y; r.m[2] = outX.z;
    r.m[4] = outY.x; r.m[5] = outY.y; r.m[6] = outY.z;
    r.m[8] = outZ.x; r.m[9] = outZ.y; r.m[10] = outZ.z;

    Vec3 gPos = getActiveGizmoPosition();
    r.m[12] = gPos.x; r.m[13] = gPos.y; r.m[14] = gPos.z;
    return r;
}

Mat4 MeshData::getModelMatrix() const {
    return Mat4::translate(position);
}

void MeshData::draw(GLESEngine& engine) {
    Mat4 model = getModelMatrix();

    // 1. رسم أوجه المجسم المصمتة
    if (faceVao && faceIndexCount > 0) {
        engine.useMeshProgram(model);
        glBindVertexArray(faceVao);
        glDrawElements(GL_TRIANGLES, faceIndexCount, GL_UNSIGNED_SHORT, 0);
    }

    // 2. رسم الحواف
    if (edgeVao && edgeVertexCount > 0) {
        engine.useLineProgram(model);
        glBindVertexArray(edgeVao);
        glDrawArrays(GL_LINES, 0, edgeVertexCount);
    }

    // 3. رسم النقاط
    if (vertDotsVao && vertDotsCount > 0) {
        engine.useLineProgram(model);
        glBindVertexArray(vertDotsVao);
        glDrawArrays(GL_LINES, 0, vertDotsCount);
    }

    glBindVertexArray(0);
}

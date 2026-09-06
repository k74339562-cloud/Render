#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return std::sqrt(dot(*this)); }
    Vec3 normalize() const {
        float l = length();
        return l > 0.00001f ? *this * (1.0f / l) : Vec3();
    }
};

struct Ray {
    Vec3 origin;
    Vec3 dir;
};

struct Mat4 {
    float m[16] = {0};

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 translate(const Vec3& v) {
        Mat4 r = Mat4::identity();
        r.m[12] = v.x;
        r.m[13] = v.y;
        r.m[14] = v.z;
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).normalize();
        Vec3 s = f.cross(up).normalize();
        Vec3 u = s.cross(f);

        Mat4 r = Mat4::identity();
        r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z; r.m[12] = -s.dot(eye);
        r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z; r.m[13] = -u.dot(eye);
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = f.dot(eye);
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.m[j * 4 + i] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    r.m[j * 4 + i] += m[k * 4 + i] * o.m[j * 4 + k];
                }
            }
        }
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        float x = m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12];
        float y = m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13];
        float z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        if (std::abs(w) > 0.00001f) {
            return {x / w, y / w, z / w};
        }
        return {x, y, z};
    }

    Mat4 inverse() const {
        Mat4 inv;
        float* dst = inv.m;
        const float* src = m;

        dst[0] = src[5]  * src[10] * src[15] - 
                 src[5]  * src[11] * src[14] - 
                 src[9]  * src[6]  * src[15] + 
                 src[9]  * src[7]  * src[14] +
                 src[13] * src[6]  * src[11] - 
                 src[13] * src[7]  * src[10];

        dst[4] = -src[4]  * src[10] * src[15] + 
                  src[4]  * src[11] * src[14] + 
                  src[8]  * src[6]  * src[15] - 
                  src[8]  * src[7]  * src[14] - 
                  src[12] * src[6]  * src[11] + 
                  src[12] * src[7]  * src[10];

        dst[8] = src[4]  * src[9] * src[15] - 
                 src[4]  * src[11] * src[13] - 
                 src[8]  * src[5] * src[15] + 
                 src[8]  * src[7] * src[13] + 
                 src[12] * src[5] * src[11] - 
                 src[12] * src[7] * src[9];

        dst[12] = -src[4]  * src[9] * src[14] + 
                   src[4]  * src[10] * src[13] + 
                   src[8]  * src[5] * src[14] - 
                   src[8]  * src[6] * src[13] - 
                   src[12] * src[5] * src[10] + 
                   src[12] * src[6] * src[9];

        dst[1] = -src[1]  * src[10] * src[15] + 
                  src[1]  * src[11] * src[14] + 
                  src[9]  * src[2] * src[15] - 
                  src[9]  * src[3] * src[14] - 
                  src[13] * src[2] * src[11] + 
                  src[13] * src[3] * src[10];

        dst[5] = src[0]  * src[10] * src[15] - 
                 src[0]  * src[11] * src[14] - 
                 src[8]  * src[2] * src[15] + 
                 src[8]  * src[3] * src[14] + 
                 src[12] * src[2] * src[11] - 
                 src[12] * src[3] * src[10];

        dst[9] = -src[0]  * src[9] * src[15] + 
                  src[0]  * src[11] * src[13] + 
                  src[8]  * src[1] * src[15] - 
                  src[8]  * src[3] * src[13] - 
                  src[12] * src[1] * src[11] + 
                  src[12] * src[3] * src[9];

        dst[13] = src[0]  * src[9] * src[14] - 
                  src[0]  * src[10] * src[13] - 
                  src[8]  * src[1] * src[14] + 
                  src[8]  * src[2] * src[13] + 
                  src[12] * src[1] * src[10] - 
                  src[12] * src[2] * src[9];

        dst[2] = src[1]  * src[6] * src[15] - 
                 src[1]  * src[7] * src[14] - 
                 src[5]  * src[2] * src[15] + 
                 src[5]  * src[3] * src[14] + 
                 src[13] * src[2] * src[7] - 
                 src[13] * src[3] * src[6];

        dst[6] = -src[0]  * src[6] * src[15] - 
                  src[0]  * src[7] * src[14] + 
                  src[4]  * src[2] * src[15] - 
                  src[4]  * src[3] * src[14] - 
                  src[12] * src[2] * src[7] + 
                  src[12] * src[3] * src[6];

        dst[10] = src[0]  * src[5] * src[15] - 
                  src[0]  * src[7] * src[13] - 
                  src[4]  * src[1] * src[15] + 
                  src[4]  * src[3] * src[13] + 
                  src[12] * src[1] * src[7] - 
                  src[12] * src[3] * src[5];

        dst[14] = -src[0]  * src[5] * src[14] + 
                   src[0]  * src[6] * src[13] + 
                   src[4]  * src[1] * src[14] - 
                   src[4]  * src[2] * src[13] - 
                   src[12] * src[1] * src[6] + 
                   src[12] * src[2] * src[5];

        dst[3] = -src[1] * src[6] * src[11] + 
                  src[1] * src[7] * src[10] + 
                  src[5] * src[2] * src[11] - 
                  src[5] * src[3] * src[10] - 
                  src[9] * src[2] * src[7] + 
                  src[9] * src[3] * src[6];

        dst[7] = src[0] * src[6] * src[11] - 
                 src[0] * src[7] * src[10] - 
                 src[4] * src[2] * src[11] + 
                 src[4] * src[3] * src[10] + 
                 src[8] * src[2] * src[7] - 
                 src[8] * src[3] * src[6];

        dst[11] = -src[0] * src[5] * src[11] + 
                   src[0] * src[7] * src[9] + 
                   src[4] * src[1] * src[11] - 
                   src[4] * src[3] * src[9] - 
                   src[8] * src[1] * src[7] + 
                   src[8] * src[3] * src[5];

        dst[15] = src[0] * src[5] * src[10] - 
                  src[0] * src[6] * src[9] - 
                  src[4] * src[1] * src[10] + 
                  src[4] * src[2] * src[9] + 
                  src[8] * src[1] * src[6] - 
                  src[8] * src[2] * src[5];

        float det = src[0] * dst[0] + src[1] * dst[4] + src[2] * dst[8] + src[3] * dst[12];
        if (std::abs(det) < 0.00001f) return Mat4::identity();

        float invDet = 1.0f / det;
        for (int i = 0; i < 16; i++) dst[i] *= invDet;
        return inv;
    }
};

#include "camera.h"
#include <algorithm>
#include <cmath>

// دوران بلندر المداري (Turntable Orbit) - يغير الزوايا فقط ولا يلمس مركز المكعب ofs
void Camera::onOrbit(float dx, float dy) {
    yaw -= dx * 0.006f;
    // قفل زاوية الارتفاع عند 89 درجة لمنع انقلاب الكاميرا (Gimbal Lock)
    pitch = std::clamp(pitch + dy * 0.006f, -1.55f, 1.55f);
}

// تقريب بلندر الأسي الناعم (Exponential Zoom)
void Camera::onZoom(float delta) {
    dist = std::clamp(dist * (1.0f - delta * 0.005f), 1.5f, 50.0f);
}

// إزاحة بلندر المتعامدة مع زاوية الكاميرا (Pan)
void Camera::onPan(float dx, float dy) {
    // حساب متجهات الرؤية المتعامدة
    float cosY = std::cos(yaw), sinY = std::sin(yaw);
    Vec3 right = {cosY, 0.0f, -sinY};
    Vec3 up = {-sinY * std::sin(pitch), std::cos(pitch), -cosY * std::sin(pitch)};

    float factor = dist * 0.0012f;
    ofs = ofs - (right * (dx * factor)) + (up * (dy * factor));
}

Vec3 Camera::getPosition() const {
    return ofs + Vec3(
        dist * std::cos(pitch) * std::sin(yaw),
        dist * std::sin(pitch),
        dist * std::cos(pitch) * std::cos(yaw)
    );
}

// معادلة مصفوفة الرؤية الرسمية في بلندر: T(0,0,-dist) * R(pitch) * R(yaw) * T(-ofs)
Mat4 Camera::getViewMatrix() const {
    Mat4 tNegOfs = Mat4::identity();
    tNegOfs.m[12] = -ofs.x;
    tNegOfs.m[13] = -ofs.y;
    tNegOfs.m[14] = -ofs.z;

    Mat4 rY = Mat4::identity();
    float cY = std::cos(yaw), sY = std::sin(yaw);
    rY.m[0] = cY;  rY.m[2] = sY;
    rY.m[8] = -sY; rY.m[10] = cY;

    Mat4 rX = Mat4::identity();
    float cP = std::cos(pitch), sP = std::sin(pitch);
    rX.m[5] = cP;  rX.m[6] = -sP;
    rX.m[9] = sP;  rX.m[10] = cP;

    Mat4 tDist = Mat4::identity();
    tDist.m[14] = -dist; // إرجاع عين الكاميرا للخلف بمقدار dist

    return tDist * (rX * (rY * tNegOfs));
}

Mat4 Camera::getProjectionMatrix(float width, float height) const {
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;
    float fovRad = 45.0f * (3.14159265f / 180.0f);
    float nearZ = 0.1f, farZ = 150.0f;

    Mat4 r;
    float tanHalf = std::tan(fovRad * 0.5f);
    r.m[0] = 1.0f / (aspect * tanHalf);
    r.m[5] = 1.0f / tanHalf;
    // مصفوفة Vulkan الدقيقة للعمق [0, 1]
    r.m[10] = farZ / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = -(farZ * nearZ) / (farZ - nearZ);
    return r;
}

#pragma once
#include "math_3d.h"
#include "render_engine.h"

class Box {
public:
    Vec3 position = {0.0f, 0.0f, 0.0f};

    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vboMemory = VK_NULL_HANDLE;
    VkBuffer ibo = VK_NULL_HANDLE;
    VkDeviceMemory iboMemory = VK_NULL_HANDLE;

    VkBuffer edgesVbo = VK_NULL_HANDLE;
    VkDeviceMemory edgesVboMemory = VK_NULL_HANDLE;

    void init(RenderEngine& engine);
    void cleanup(RenderEngine& engine);
    void draw(RenderEngine& engine);

    Mat4 getModelMatrix() const;
};

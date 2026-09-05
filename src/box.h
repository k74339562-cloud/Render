#pragma once
#include <vulkan/vulkan.h>
#include "math_3d.h"

class Box {
public:
    Vec3 position = {0.0f, 0.0f, 0.0f};

    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vboMemory = VK_NULL_HANDLE;
    VkBuffer ibo = VK_NULL_HANDLE;
    VkDeviceMemory iboMemory = VK_NULL_HANDLE;

    VkBuffer edgesVbo = VK_NULL_HANDLE;
    VkDeviceMemory edgesVboMemory = VK_NULL_HANDLE;

    void init(VkDevice device, VkPhysicalDevice physicalDevice);
    void cleanup(VkDevice device);
    void renderFaces(VkCommandBuffer cmd);
    void renderEdges(VkCommandBuffer cmd);
    Mat4 getModelMatrix() const;

private:
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, 
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
};

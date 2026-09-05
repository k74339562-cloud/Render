#include "box.h"
#include <cstring>

struct VertexCube { float x, y, z; float nx, ny, nz; };
struct VertexLine { float x, y, z; float r, g, b, a; };

uint32_t Box::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void Box::createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, 
                       VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                       VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, properties);
    vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Box::init(VkDevice device, VkPhysicalDevice physicalDevice) {
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

    createBuffer(device, physicalDevice, sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vbo, vboMemory);
    void* vData; vkMapMemory(device, vboMemory, 0, sizeof(cubeVertices), 0, &vData);
    memcpy(vData, cubeVertices, sizeof(cubeVertices)); vkUnmapMemory(device, vboMemory);

    createBuffer(device, physicalDevice, sizeof(cubeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ibo, iboMemory);
    void* iData; vkMapMemory(device, iboMemory, 0, sizeof(cubeIndices), 0, &iData);
    memcpy(iData, cubeIndices, sizeof(cubeIndices)); vkUnmapMemory(device, iboMemory);

    // حواف التحديد البرتقالية (#E86C19)
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
    createBuffer(device, physicalDevice, sizeof(cubeEdges), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, edgesVbo, edgesVboMemory);
    void* edgeData; vkMapMemory(device, edgesVboMemory, 0, sizeof(cubeEdges), 0, &edgeData);
    memcpy(edgeData, cubeEdges, sizeof(cubeEdges)); vkUnmapMemory(device, edgesVboMemory);
}

void Box::cleanup(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;
    vkDestroyBuffer(device, edgesVbo, nullptr); vkFreeMemory(device, edgesVboMemory, nullptr);
    vkDestroyBuffer(device, ibo, nullptr); vkFreeMemory(device, iboMemory, nullptr);
    vkDestroyBuffer(device, vbo, nullptr); vkFreeMemory(device, vboMemory, nullptr);
    vbo = ibo = edgesVbo = VK_NULL_HANDLE;
}

void Box::renderFaces(VkCommandBuffer cmd) {
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, offsets);
    vkCmdBindIndexBuffer(cmd, ibo, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
}

void Box::renderEdges(VkCommandBuffer cmd) {
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &edgesVbo, offsets);
    vkCmdDraw(cmd, 24, 1, 0, 0);
}

Mat4 Box::getModelMatrix() const {
    return Mat4::translate(position);
}

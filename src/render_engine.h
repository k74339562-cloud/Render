#pragma once
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <vector>
#include "math_3d.h"
#include "vulkan_pipeline.h"

// ترويسة UBO الكاميرا العالمية فقط
struct GlobalUniformObject {
    Mat4 viewProj;
    Vec3 camPos;
    float padding = 0.0f;
};

class RenderEngine {
public:
    static const int MAX_FRAMES_IN_FLIGHT = 2; // تقنية جودوت لتعدد الإطارات ومنع التقطيع

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D swapchainExtent = {0, 0};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VulkanPipeline pipeline;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // أدوات المزامنة لتقنية الإطارات المتزامنة (Frames-in-Flight)
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // ذاكرة UBO مستقلة لكل فريم متزامن لمنع سباق الذاكرة
    std::vector<VkBuffer> uboBuffers;
    std::vector<VkDeviceMemory> uboBufferMemories;
    std::vector<void*> uboMapped;

    uint32_t currentFrameIndex = 0;
    uint32_t currentImageIndex = 0;

    // دوال دورة حياة المحرك
    bool init(ANativeWindow* window);
    void cleanup();

    // واجهة الرسم النقية للأدوات والمجسمات (Pure Draw API)
    bool beginFrame(const Mat4& viewMatrix, const Mat4& projMatrix, const Vec3& camPos);
    void drawMesh(VkBuffer vbo, VkBuffer ibo, uint32_t indexCount, const Mat4& model);
    void drawLines(VkBuffer vbo, uint32_t vertexCount, const Mat4& model);
    void drawOverlayLines(VkBuffer vbo, uint32_t vertexCount, const Mat4& model);
    void drawGizmo(VkBuffer vbo, uint32_t vertexCount, const Mat4& model);
    void endFrame();

    // دوال مساعدة لإنشاء وحذف البافرات
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory);

private:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

#pragma once
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <vector>
#include "camera.h"
#include "gizmo.h"
#include "vulkan_pipeline.h"

enum GizmoAxis {
    AXIS_NONE = 0,
    AXIS_X,
    AXIS_Y,
    AXIS_Z
};

struct UniformBufferObject {
    Mat4 viewProj;
    Mat4 model;
    Vec3 camPos;
    float padding = 0.0f;
};

class VulkanRenderer {
public:
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
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    VkBuffer uboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uboBufferMemory = VK_NULL_HANDLE;

    VkBuffer cubeVbo = VK_NULL_HANDLE;
    VkDeviceMemory cubeVboMemory = VK_NULL_HANDLE;
    VkBuffer cubeIbo = VK_NULL_HANDLE;
    VkDeviceMemory cubeIboMemory = VK_NULL_HANDLE;

    VkBuffer cubeEdgesVbo = VK_NULL_HANDLE;
    VkDeviceMemory cubeEdgesVboMemory = VK_NULL_HANDLE;

    VkBuffer gridVbo = VK_NULL_HANDLE;
    VkDeviceMemory gridVboMemory = VK_NULL_HANDLE;
    uint32_t gridVertexCount = 0;

    VkBuffer gizmoVbo = VK_NULL_HANDLE;
    VkDeviceMemory gizmoVboMemory = VK_NULL_HANDLE;
    uint32_t gizmoVertexCount = 0;

    Camera camera;
    Gizmo gizmo;

    // موقع المكعب في الفضاء وحالته التفاعلية
    Vec3 cubePosition = {0.0f, 0.0f, 0.0f};
    GizmoAxis activeAxis = AXIS_NONE;

    bool init(ANativeWindow* window);
    void cleanup();
    void renderFrame();

    // دوال فحص التقاطع والتحريك التفاعلي
    GizmoAxis testGizmoHit(const Ray& ray);
    void dragGizmo(const Ray& rayPrev, const Ray& rayCurr);

private:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
};

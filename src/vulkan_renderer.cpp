#include "vulkan_renderer.h"
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <android/native_window.h>
#include <vector>
#include <cstring>
#include <cmath>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanRenderer", __VA_ARGS__))

struct VertexCube { float x, y, z; float nx, ny, nz; };
struct VertexLine { float x, y, z; float r, g, b, a; };

static void* g_uboMapped = nullptr;

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

// خوارزمية قياس أقرب مسافة بين شعاع اللمس وأعمدة الجزمو (Ray-to-Segment Distance)
static float distRayToSegment(const Ray& ray, const Vec3& p0, const Vec3& p1, float& outDistOnSegment) {
    Vec3 u = ray.dir;
    Vec3 v = p1 - p0;
    Vec3 w0 = ray.origin - p0;

    float a = u.dot(u);
    float b = u.dot(v);
    float c = v.dot(v);
    float d = u.dot(w0);
    float e = v.dot(w0);

    float denom = a * c - b * b;
    if (std::abs(denom) < 0.0001f) {
        outDistOnSegment = 0.0f;
        return (ray.origin - p0).length();
    }

    float sc = (b * e - c * d) / denom;
    float tc = (a * e - b * d) / denom;

    if (sc < 0.0f) sc = 0.0f; // أمام الكاميرا فقط
    tc = std::clamp(tc, 0.0f, 1.0f); // داخل حدود السهم

    outDistOnSegment = tc;
    Vec3 closestRay = ray.origin + u * sc;
    Vec3 closestSeg = p0 + v * tc;
    return (closestRay - closestSeg).length();
}

GizmoAxis VulkanRenderer::testGizmoHit(const Ray& ray) {
    float shaftLen = 1.9f;
    float grabThreshold = 0.35f; // حساسية لمس مريحة للشاشات الصغيرة

    float distOnSeg = 0.0f;
    float distX = distRayToSegment(ray, cubePosition, cubePosition + Vec3(shaftLen, 0, 0), distOnSeg);
    float distY = distRayToSegment(ray, cubePosition, cubePosition + Vec3(0, shaftLen, 0), distOnSeg);
    float distZ = distRayToSegment(ray, cubePosition, cubePosition + Vec3(0, 0, shaftLen), distOnSeg);

    GizmoAxis hit = AXIS_NONE;
    float minDist = grabThreshold;

    if (distX < minDist) { minDist = distX; hit = AXIS_X; }
    if (distY < minDist) { minDist = distY; hit = AXIS_Y; }
    if (distZ < minDist) { minDist = distZ; hit = AXIS_Z; }

    return hit;
}

void VulkanRenderer::dragGizmo(const Ray& rayPrev, const Ray& rayCurr) {
    if (activeAxis == AXIS_NONE) return;

    Vec3 axisDir = {0, 0, 0};
    if (activeAxis == AXIS_X) axisDir = {1.0f, 0.0f, 0.0f};
    if (activeAxis == AXIS_Y) axisDir = {0.0f, 1.0f, 0.0f};
    if (activeAxis == AXIS_Z) axisDir = {0.0f, 0.0f, 1.0f};

    float shaftLen = 100.0f; // امتداد لا نهائي لمحور السحب أثناء الإمساك به
    float tcPrev = 0.0f, tcCurr = 0.0f;
    distRayToSegment(rayPrev, cubePosition, cubePosition + axisDir * shaftLen, tcPrev);
    distRayToSegment(rayCurr, cubePosition, cubePosition + axisDir * shaftLen, tcCurr);

    float delta = (tcCurr - tcPrev) * shaftLen;
    cubePosition = cubePosition + (axisDir * delta);
}

bool VulkanRenderer::init(ANativeWindow* window) {
    if (device != VK_NULL_HANDLE) cleanup();

    const char* ext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.enabledExtensionCount = 2; ici.ppEnabledExtensionNames = ext;
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) return false;

    VkAndroidSurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    sci.window = window;
    if (vkCreateAndroidSurfaceKHR(instance, &sci, nullptr, &surface) != VK_SUCCESS) return false;

    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(instance, &devCount, devs.data());
    physicalDevice = devs[0];

    uint32_t qFamCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qFamCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qFamCount, qProps.data());
    uint32_t graphicsQueueFamily = 0;
    for (uint32_t i = 0; i < qFamCount; ++i) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
            graphicsQueueFamily = i;
            break;
        }
    }

    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = graphicsQueueFamily; qci.queueCount = 1; qci.pQueuePriorities = &qp;

    const char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = devExt;
    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_SRGB) {
            chosenFormat = f;
            break;
        }
    }
    swapchainFormat = chosenFormat.format;
    ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);

    swapchainExtent = { (uint32_t)ANativeWindow_getWidth(window), (uint32_t)ANativeWindow_getHeight(window) };
    VkSwapchainCreateInfoKHR swci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swci.surface = surface; swci.minImageCount = 2; swci.imageFormat = swapchainFormat;
    swci.imageColorSpace = chosenFormat.colorSpace; swci.imageExtent = swapchainExtent;
    swci.imageArrayLayers = 1; swci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; swci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; swci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    vkCreateSwapchainKHR(device, &swci, nullptr, &swapchain);

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imgCount, nullptr);
    swapchainImages.resize(imgCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imgCount, swapchainImages.data());

    swapchainImageViews.resize(imgCount);
    for (size_t i = 0; i < imgCount; i++) {
        VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = swapchainImages[i]; ivci.viewType = VK_IMAGE_VIEW_TYPE_2D; ivci.format = swapchainFormat;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; ivci.subresourceRange.levelCount = 1; ivci.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &ivci, nullptr, &swapchainImageViews[i]);
    }

    createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uboBuffer, uboBufferMemory);
    vkMapMemory(device, uboBufferMemory, 0, sizeof(UniformBufferObject), 0, &g_uboMapped);

    pipeline.init(device, physicalDevice, swapchainFormat, swapchainExtent, swapchainImageViews, uboBuffer);

    // 1. هندسة المكعب
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

    createBuffer(sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeVbo, cubeVboMemory);
    void* vData; vkMapMemory(device, cubeVboMemory, 0, sizeof(cubeVertices), 0, &vData);
    memcpy(vData, cubeVertices, sizeof(cubeVertices)); vkUnmapMemory(device, cubeVboMemory);

    createBuffer(sizeof(cubeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeIbo, cubeIboMemory);
    void* iData; vkMapMemory(device, cubeIboMemory, 0, sizeof(cubeIndices), 0, &iData);
    memcpy(iData, cubeIndices, sizeof(cubeIndices)); vkUnmapMemory(device, cubeIboMemory);

    // 2. خطوط حواف تحديد بلندر البرتقالية (#E86C19)
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
    createBuffer(sizeof(cubeEdges), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeEdgesVbo, cubeEdgesVboMemory);
    void* edgeData; vkMapMemory(device, cubeEdgesVboMemory, 0, sizeof(cubeEdges), 0, &edgeData);
    memcpy(edgeData, cubeEdges, sizeof(cubeEdges)); vkUnmapMemory(device, cubeEdgesVboMemory);

    // 3. شبكة أرضية بلندر
    std::vector<VertexLine> gridLines;
    int gridSize = 20;
    float maxDist = (float)gridSize;

    for (int i = -gridSize; i <= gridSize; ++i) {
        float fi = (float)i;
        float d = std::abs(fi) / maxDist;
        float alpha = std::pow(1.0f - d, 1.8f) * 0.40f;

        if (i == 0) {
            gridLines.push_back({-maxDist, 0.0f, -1.0f, 0.92f, 0.23f, 0.32f, 0.95f});
            gridLines.push_back({ maxDist, 0.0f, -1.0f, 0.92f, 0.23f, 0.32f, 0.95f});
            gridLines.push_back({0.0f, -maxDist, -1.0f, 0.51f, 0.78f, 0.14f, 0.95f});
            gridLines.push_back({0.0f,  maxDist, -1.0f, 0.51f, 0.78f, 0.14f, 0.95f});
            continue;
        }

        float gc = 0.29f;
        gridLines.push_back({-maxDist, fi, -1.0f, gc, gc, gc, alpha});
        gridLines.push_back({ maxDist, fi, -1.0f, gc, gc, gc, alpha});

        gridLines.push_back({fi, -maxDist, -1.0f, gc, gc, gc, alpha});
        gridLines.push_back({fi,  maxDist, -1.0f, gc, gc, gc, alpha});
    }
    gridVertexCount = (uint32_t)gridLines.size();

    createBuffer(gridLines.size() * sizeof(VertexLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gridVbo, gridVboMemory);
    void* gLineData; vkMapMemory(device, gridVboMemory, 0, gridLines.size() * sizeof(VertexLine), 0, &gLineData);
    memcpy(gLineData, gridLines.data(), gridLines.size() * sizeof(VertexLine)); vkUnmapMemory(device, gridVboMemory);

    // 4. تهيئة جزمو بلندر
    gizmo.init();
    gizmoVertexCount = (uint32_t)gizmo.vertices.size();
    createBuffer(gizmo.vertices.size() * sizeof(GizmoVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gizmoVbo, gizmoVboMemory);
    void* gizmoData; vkMapMemory(device, gizmoVboMemory, 0, gizmo.vertices.size() * sizeof(GizmoVertex), 0, &gizmoData);
    memcpy(gizmoData, gizmo.vertices.data(), gizmo.vertices.size() * sizeof(GizmoVertex)); vkUnmapMemory(device, gizmoVboMemory);

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = graphicsQueueFamily; cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device, &cpci, nullptr, &commandPool);

    commandBuffers.resize(imgCount);
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = commandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = imgCount;
    vkAllocateCommandBuffers(device, &cbai, commandBuffers.data());

    VkSemaphoreCreateInfo sciSync{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(device, &sciSync, nullptr, &imageAvailableSemaphore);
    vkCreateSemaphore(device, &sciSync, nullptr, &renderFinishedSemaphore);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fci, nullptr, &inFlightFence);

    LOGI("Blender Pure Replica Engine Booted Successfully!");
    return true;
}

void VulkanRenderer::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        if (g_uboMapped) { vkUnmapMemory(device, uboBufferMemory); g_uboMapped = nullptr; }

        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);

        pipeline.cleanup(device);

        vkDestroyBuffer(device, gizmoVbo, nullptr); vkFreeMemory(device, gizmoVboMemory, nullptr);
        vkDestroyBuffer(device, gridVbo, nullptr); vkFreeMemory(device, gridVboMemory, nullptr);
        vkDestroyBuffer(device, cubeEdgesVbo, nullptr); vkFreeMemory(device, cubeEdgesVboMemory, nullptr);
        vkDestroyBuffer(device, cubeIbo, nullptr); vkFreeMemory(device, cubeIboMemory, nullptr);
        vkDestroyBuffer(device, cubeVbo, nullptr); vkFreeMemory(device, cubeVboMemory, nullptr);
        vkDestroyBuffer(device, uboBuffer, nullptr); vkFreeMemory(device, uboBufferMemory, nullptr);

        for (auto iv : swapchainImageViews) vkDestroyImageView(device, iv, nullptr);
        swapchainImageViews.clear();
        swapchainImages.clear();
        commandBuffers.clear();

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
    }

    if (instance != VK_NULL_HANDLE) {
        if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    device = VK_NULL_HANDLE; instance = VK_NULL_HANDLE; surface = VK_NULL_HANDLE;
}

void VulkanRenderer::renderFrame() {
    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE) return;

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return;

    vkResetFences(device, 1, &inFlightFence);

    VkCommandBuffer cmd = commandBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &cbbi);

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.204f, 0.204f, 0.204f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = pipeline.renderPass;
    rpbi.framebuffer = pipeline.framebuffers[imageIndex];
    rpbi.renderArea.extent = swapchainExtent;
    rpbi.clearValueCount = 2; rpbi.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkDeviceSize offsets[] = {0};

    // 1. رسم شبكة الأرضية الثابتة عند نقطة الأصل
    if (g_uboMapped) {
        UniformBufferObject uboGrid{};
        Mat4 v = camera.getViewMatrix();
        Mat4 p = camera.getProjectionMatrix((float)swapchainExtent.width, (float)swapchainExtent.height);
        uboGrid.viewProj = p * v;
        uboGrid.model = Mat4::identity();
        uboGrid.camPos = camera.getPosition();
        memcpy(g_uboMapped, &uboGrid, sizeof(uboGrid));
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &gridVbo, offsets);
    vkCmdDraw(cmd, gridVertexCount, 1, 0, 0);

    // 2. تحديث موقع المكعب والجزمو في الفضاء معاً (Model Matrix Translation)
    if (g_uboMapped) {
        UniformBufferObject uboCube{};
        Mat4 v = camera.getViewMatrix();
        Mat4 p = camera.getProjectionMatrix((float)swapchainExtent.width, (float)swapchainExtent.height);
        uboCube.viewProj = p * v;
        uboCube.model = Mat4::translate(cubePosition);
        uboCube.camPos = camera.getPosition();
        memcpy(g_uboMapped, &uboCube, sizeof(uboCube));
    }

    // رسم المكعب المصمت
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.cubePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVbo, offsets);
    vkCmdBindIndexBuffer(cmd, cubeIbo, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

    // رسم حواف تحديد بلندر البرتقالية المتحركة مع المكعب
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &cubeEdgesVbo, offsets);
    vkCmdDraw(cmd, 24, 1, 0, 0);

    // رسم جزمو بلندر المتحرك مع المكعب
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.gizmoPipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &gizmoVbo, offsets);
    vkCmdDraw(cmd, gizmoVertexCount, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSems[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = waitSems; si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VkSemaphore sigSems[] = {renderFinishedSemaphore};
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = sigSems;

    vkQueueSubmit(graphicsQueue, 1, &si, inFlightFence);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = sigSems;
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain; pi.pImageIndices = &imageIndex;
    vkQueuePresentKHR(graphicsQueue, &pi);
}

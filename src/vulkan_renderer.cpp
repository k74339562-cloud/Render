#include "vulkan_renderer.h"
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <android/native_window.h>
#include <vector>
#include <cstring>

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

    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = 0; qci.queueCount = 1; qci.pQueuePriorities = &qp;

    const char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = devExt;
    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(device, 0, 0, &graphicsQueue);

    swapchainExtent = { (uint32_t)ANativeWindow_getWidth(window), (uint32_t)ANativeWindow_getHeight(window) };
    VkSwapchainCreateInfoKHR swci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swci.surface = surface; swci.minImageCount = 2; swci.imageFormat = swapchainFormat;
    swci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; swci.imageExtent = swapchainExtent;
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

    // 1. هندسة المكعب بنظام Z-Up
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

    // 2. حواف مكعب بلندر الـ 12 الداكنة (#1C1C1C) لجعله مجسماً وحاداً
    VertexLine cubeEdges[] = {
        {-1,-1,-1, 0.11f,0.11f,0.11f,1}, { 1,-1,-1, 0.11f,0.11f,0.11f,1},
        { 1,-1,-1, 0.11f,0.11f,0.11f,1}, { 1, 1,-1, 0.11f,0.11f,0.11f,1},
        { 1, 1,-1, 0.11f,0.11f,0.11f,1}, {-1, 1,-1, 0.11f,0.11f,0.11f,1},
        {-1, 1,-1, 0.11f,0.11f,0.11f,1}, {-1,-1,-1, 0.11f,0.11f,0.11f,1},
        {-1,-1, 1, 0.11f,0.11f,0.11f,1}, { 1,-1, 1, 0.11f,0.11f,0.11f,1},
        { 1,-1, 1, 0.11f,0.11f,0.11f,1}, { 1, 1, 1, 0.11f,0.11f,0.11f,1},
        { 1, 1, 1, 0.11f,0.11f,0.11f,1}, {-1, 1, 1, 0.11f,0.11f,0.11f,1},
        {-1, 1, 1, 0.11f,0.11f,0.11f,1}, {-1,-1, 1, 0.11f,0.11f,0.11f,1},
        {-1,-1,-1, 0.11f,0.11f,0.11f,1}, {-1,-1, 1, 0.11f,0.11f,0.11f,1},
        { 1,-1,-1, 0.11f,0.11f,0.11f,1}, { 1,-1, 1, 0.11f,0.11f,0.11f,1},
        { 1, 1,-1, 0.11f,0.11f,0.11f,1}, { 1, 1, 1, 0.11f,0.11f,0.11f,1},
        {-1, 1,-1, 0.11f,0.11f,0.11f,1}, {-1, 1, 1, 0.11f,0.11f,0.11f,1}
    };
    createBuffer(sizeof(cubeEdges), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeEdgesVbo, cubeEdgesVboMemory);
    void* edgeData; vkMapMemory(device, cubeEdgesVboMemory, 0, sizeof(cubeEdges), 0, &edgeData);
    memcpy(edgeData, cubeEdges, sizeof(cubeEdges)); vkUnmapMemory(device, cubeEdgesVboMemory);

    // 3. شبكة أرضية بلندر على مسطح XY عند Z = -1.0
    std::vector<VertexLine> gridLines;
    int gridSize = 14;
    for (int i = -gridSize; i <= gridSize; ++i) {
        float fi = (float)i, fs = (float)gridSize;
        float r = 0.23f, g = 0.23f, b = 0.23f;
        if (i == 0) { r = 0.90f; g = 0.22f; b = 0.22f; } // محور X الأحمر
        gridLines.push_back({-fs, fi, -1.0f, r, g, b, 1.0f});
        gridLines.push_back({ fs, fi, -1.0f, r, g, b, 1.0f});

        r = 0.23f; g = 0.23f; b = 0.23f;
        if (i == 0) { r = 0.26f; g = 0.63f; b = 0.28f; } // محور Y الأخضر
        gridLines.push_back({fi, -fs, -1.0f, r, g, b, 1.0f});
        gridLines.push_back({fi,  fs, -1.0f, r, g, b, 1.0f});
    }
    gridVertexCount = (uint32_t)gridLines.size();

    createBuffer(gridLines.size() * sizeof(VertexLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gridVbo, gridVboMemory);
    void* gLineData; vkMapMemory(device, gridVboMemory, 0, gridLines.size() * sizeof(VertexLine), 0, &gLineData);
    memcpy(gLineData, gridLines.data(), gridLines.size() * sizeof(VertexLine)); vkUnmapMemory(device, gridVboMemory);

    // 4. تهيئة جزمو بلندر Z-Up
    gizmo.init();
    gizmoVertexCount = (uint32_t)gizmo.vertices.size();
    createBuffer(gizmo.vertices.size() * sizeof(GizmoVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gizmoVbo, gizmoVboMemory);
    void* gizmoData; vkMapMemory(device, gizmoVboMemory, 0, gizmo.vertices.size() * sizeof(GizmoVertex), 0, &gizmoData);
    memcpy(gizmoData, gizmo.vertices.data(), gizmo.vertices.size() * sizeof(GizmoVertex)); vkUnmapMemory(device, gizmoVboMemory);

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = 0; cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

    LOGI("نجح إقلاع محرك ريندر بلندر AAA المقسم!");
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
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    if (vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS) return;

    if (g_uboMapped) {
        UniformBufferObject ubo{};
        Mat4 v = camera.getViewMatrix();
        Mat4 p = camera.getProjectionMatrix((float)swapchainExtent.width, (float)swapchainExtent.height);
        ubo.viewProj = p * v;
        ubo.model = Mat4::identity();
        ubo.camPos = camera.getPosition();
        memcpy(g_uboMapped, &ubo, sizeof(ubo));
    }

    VkCommandBuffer cmd = commandBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &cbbi);

    // خلفية استوديو بلندر الرمادية الأصلية (#303030)
    VkClearValue clearValues[2];
    clearValues[0].color = {{0.188f, 0.188f, 0.188f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = pipeline.renderPass;
    rpbi.framebuffer = pipeline.framebuffers[imageIndex];
    rpbi.renderArea.extent = swapchainExtent;
    rpbi.clearValueCount = 2; rpbi.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkDeviceSize offsets[] = {0};

    // 1. رسم شبكة أرضية بلندر على مسطح XY
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &gridVbo, offsets);
    vkCmdDraw(cmd, gridVertexCount, 1, 0, 0);

    // 2. رسم المكعب المصمت بشيدر طين بلندر العميق
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.cubePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVbo, offsets);
    vkCmdBindIndexBuffer(cmd, cubeIbo, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

    // 3. رسم حواف مكعب بلندر الـ 12 الداكنة لإبراز الزوايا بحدة
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &cubeEdgesVbo, offsets);
    vkCmdDraw(cmd, 24, 1, 0, 0);

    // 4. رسم جزمو بلندر الرسمي Z-Up (الأزرق للأعلى، الأحمر لليمين، الأخضر للأمام)
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

#include "vulkan_renderer.h"
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <android/native_window.h>
#include <vector>
#include <cstring>
#include <cmath>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanRenderer", __VA_ARGS__))

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

static float distToScreenSegment(float tx, float ty, const Vec2& p0, const Vec2& p1) {
    float vx = p1.x - p0.x, vy = p1.y - p0.y;
    float wx = tx - p0.x,   wy = ty - p0.y;

    float c1 = wx * vx + wy * vy;
    if (c1 <= 0.0f) return std::sqrt(wx * wx + wy * wy);

    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        float dx = tx - p1.x, dy = ty - p1.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    float b = c1 / c2;
    float px = p0.x + b * vx, py = p0.y + b * vy;
    float dx = tx - px, dy = ty - py;
    return std::sqrt(dx * dx + dy * dy);
}

GizmoAxis VulkanRenderer::testGizmoHit(float touchX, float touchY, float screenW, float screenH) {
    float shaftLen = 1.8f;
    Vec2 pCenter = camera.projectToScreen(box.position, screenW, screenH);
    Vec2 pX = camera.projectToScreen(box.position + Vec3(shaftLen, 0, 0), screenW, screenH);
    Vec2 pY = camera.projectToScreen(box.position + Vec3(0, shaftLen, 0), screenW, screenH);
    Vec2 pZ = camera.projectToScreen(box.position + Vec3(0, 0, shaftLen), screenW, screenH);

    float dX = distToScreenSegment(touchX, touchY, pCenter, pX);
    float dY = distToScreenSegment(touchX, touchY, pCenter, pY);
    float dZ = distToScreenSegment(touchX, touchY, pCenter, pZ);

    float threshold = 48.0f;
    GizmoAxis hit = AXIS_NONE;
    float minDist = threshold;

    if (dZ < minDist) { minDist = dZ; hit = AXIS_Z; }
    if (dY < minDist) { minDist = dY; hit = AXIS_Y; }
    if (dX < minDist) { minDist = dX; hit = AXIS_X; }

    return hit;
}

void VulkanRenderer::dragGizmo(float dx, float dy, float screenW, float screenH) {
    if (activeAxis == AXIS_NONE) return;

    Vec3 axisDir3D = {0, 0, 0};
    if (activeAxis == AXIS_X) axisDir3D = {1.0f, 0.0f, 0.0f};
    if (activeAxis == AXIS_Y) axisDir3D = {0.0f, 1.0f, 0.0f};
    if (activeAxis == AXIS_Z) axisDir3D = {0.0f, 0.0f, 1.0f};

    Vec2 pCenter = camera.projectToScreen(box.position, screenW, screenH);
    Vec2 pTip    = camera.projectToScreen(box.position + axisDir3D, screenW, screenH);

    float screenDirX = pTip.x - pCenter.x;
    float screenDirY = pTip.y - pCenter.y;
    float len = std::sqrt(screenDirX * screenDirX + screenDirY * screenDirY);
    if (len < 0.001f) return;

    screenDirX /= len;
    screenDirY /= len;

    float dotMove = (dx * screenDirX) + (dy * screenDirY);
    float camDist = (camera.pos - box.position).length();
    float worldUnitsPerPixel = (camDist * 0.0016f);

    // تحريك المكعب المستقل فقط دون لمس شبكة الأرضية
    box.position = box.position + (axisDir3D * (dotMove * worldUnitsPerPixel));
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

    // تهيئة المكعب المستقل
    box.init(device, physicalDevice);

    // تهيئة شبكة الأرضية
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

    // تهيئة الجزمو
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

    LOGI("Modular Engine Started!");
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

        box.cleanup(device);
        pipeline.cleanup(device);

        vkDestroyBuffer(device, gizmoVbo, nullptr); vkFreeMemory(device, gizmoVboMemory, nullptr);
        vkDestroyBuffer(device, gridVbo, nullptr); vkFreeMemory(device, gridVboMemory, nullptr);
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

    // تحديث الكاميرا العامة في الـ UBO مرة واحدة فقط في الفريم
    if (g_uboMapped) {
        UniformBufferObject ubo{};
        Mat4 v = camera.getViewMatrix();
        Mat4 p = camera.getProjectionMatrix((float)swapchainExtent.width, (float)swapchainExtent.height);
        ubo.viewProj = p * v;
        ubo.camPos = camera.pos;
        memcpy(g_uboMapped, &ubo, sizeof(ubo));
    }

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

    // 1. رسم شبكة الأرضية الثابتة للأبد عند نقطة الأصل (Identity Push Constant)
    Mat4 gridModel = Mat4::identity();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &gridModel);
    vkCmdBindVertexBuffers(cmd, 0, 1, &gridVbo, offsets);
    vkCmdDraw(cmd, gridVertexCount, 1, 0, 0);

    // 2. رسم المكعب المستقل (Box) بمصفوفته الخاصة عبر Push Constants
    Mat4 boxModel = box.getModelMatrix();

    // رسم أوجه المكعب
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.cubePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &boxModel);
    box.renderFaces(cmd);

    // رسم حواف تحديد المكعب البرتقالية
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &boxModel);
    box.renderEdges(cmd);

    // 3. رسم الجزمو بمصفوفة المكعب ليتحرك معه كقطعة واحدة
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.gizmoPipeline);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &boxModel);
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

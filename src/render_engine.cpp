#include "render_engine.h"
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <vector>
#include <cstring>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "RenderEngine", __VA_ARGS__))

uint32_t RenderEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void RenderEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
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

void RenderEngine::destroyBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory) {
    if (device != VK_NULL_HANDLE) {
        if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, nullptr);
        if (bufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, bufferMemory, nullptr);
    }
}

bool RenderEngine::init(ANativeWindow* window) {
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

    // بناء الـ UBOs المتعددة لدعم تعدد الإطارات دون أي سباق ذاكرة
    uboBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uboBufferMemories.resize(MAX_FRAMES_IN_FLIGHT);
    uboMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(sizeof(GlobalUniformObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     uboBuffers[i], uboBufferMemories[i]);
        vkMapMemory(device, uboBufferMemories[i], 0, sizeof(GlobalUniformObject), 0, &uboMapped[i]);
    }

    pipeline.init(device, physicalDevice, swapchainFormat, swapchainExtent, swapchainImageViews, uboBuffers[0]);

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = graphicsQueueFamily; cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device, &cpci, nullptr, &commandPool);

    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = commandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    vkAllocateCommandBuffers(device, &cbai, commandBuffers.data());

    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkSemaphoreCreateInfo sciSync{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(device, &sciSync, nullptr, &imageAvailableSemaphores[i]);
        vkCreateSemaphore(device, &sciSync, nullptr, &renderFinishedSemaphores[i]);

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device, &fci, nullptr, &inFlightFences[i]);
    }

    LOGI("Modular RenderEngine Initialized with Double-Buffering!");
    return true;
}

void RenderEngine::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (uboMapped[i]) vkUnmapMemory(device, uboBufferMemories[i]);
            destroyBuffer(uboBuffers[i], uboBufferMemories[i]);
            vkDestroyFence(device, inFlightFences[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
        uboMapped.clear();

        vkDestroyCommandPool(device, commandPool, nullptr);
        pipeline.cleanup(device);

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

bool RenderEngine::beginFrame(const Mat4& viewMatrix, const Mat4& projMatrix, const Vec3& camPos) {
    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE) return false;

    vkWaitForFences(device, 1, &inFlightFences[currentFrameIndex], VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrameIndex], VK_NULL_HANDLE, &currentImageIndex);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return false;

    vkResetFences(device, 1, &inFlightFences[currentFrameIndex]);

    // تحديث بيانات الكاميرا في UBO الفريم الحالي
    GlobalUniformObject ubo{};
    ubo.viewProj = projMatrix * viewMatrix;
    ubo.camPos = camPos;
    memcpy(uboMapped[currentFrameIndex], &ubo, sizeof(ubo));

    VkCommandBuffer cmd = commandBuffers[currentFrameIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &cbbi);

    // خلفية استوديو بلندر الأصلية (#343434)
    VkClearValue clearValues[2];
    clearValues[0].color = {{0.204f, 0.204f, 0.204f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = pipeline.renderPass;
    rpbi.framebuffer = pipeline.framebuffers[currentImageIndex];
    rpbi.renderArea.extent = swapchainExtent;
    rpbi.clearValueCount = 2; rpbi.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    return true;
}

void RenderEngine::drawMesh(VkBuffer vbo, VkBuffer ibo, uint32_t indexCount, const Mat4& model) {
    VkCommandBuffer cmd = commandBuffers[currentFrameIndex];
    VkDeviceSize offsets[] = {0};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.cubePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &model);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, offsets);
    vkCmdBindIndexBuffer(cmd, ibo, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

void RenderEngine::drawLines(VkBuffer vbo, uint32_t vertexCount, const Mat4& model) {
    VkCommandBuffer cmd = commandBuffers[currentFrameIndex];
    VkDeviceSize offsets[] = {0};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.linePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &model);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, offsets);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}

void RenderEngine::drawOverlayLines(VkBuffer vbo, uint32_t vertexCount, const Mat4& model) {
    drawLines(vbo, vertexCount, model);
}

void RenderEngine::drawGizmo(VkBuffer vbo, uint32_t vertexCount, const Mat4& model) {
    VkCommandBuffer cmd = commandBuffers[currentFrameIndex];
    VkDeviceSize offsets[] = {0};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.gizmoPipeline);
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &model);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbo, offsets);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}

void RenderEngine::endFrame() {
    VkCommandBuffer cmd = commandBuffers[currentFrameIndex];
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSems[] = {imageAvailableSemaphores[currentFrameIndex]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = waitSems; si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VkSemaphore sigSems[] = {renderFinishedSemaphores[currentFrameIndex]};
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = sigSems;

    vkQueueSubmit(graphicsQueue, 1, &si, inFlightFences[currentFrameIndex]);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = sigSems;
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain; pi.pImageIndices = &currentImageIndex;
    vkQueuePresentKHR(graphicsQueue, &pi);

    currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

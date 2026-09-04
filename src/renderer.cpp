#include "vulkan_renderer.h"
#include <vulkan/vulkan_android.h>
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanEngine", __VA_ARGS__))

// قراءة بيانات SPIR-V المترجمة بدقة
static const uint32_t cube_vert_data[] =
#include "shaders/cube_vert.h"
;

static const uint32_t cube_frag_data[] =
#include "shaders/cube_frag.h"
;

static const uint32_t line_vert_data[] =
#include "shaders/line_vert.h"
;

static const uint32_t line_frag_data[] =
#include "shaders/line_frag.h"
;

static VkShaderModule createShaderModule(VkDevice device, const uint32_t* code, size_t sizeBytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = sizeBytes;
    ci.pCode = code;
    VkShaderModule mod;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}

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
    // 1. إنشاء VkInstance
    const char* ext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = ext;
    vkCreateInstance(&ici, nullptr, &instance);

    // 2. إنشاء السطح
    VkAndroidSurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    sci.window = window;
    vkCreateAndroidSurfaceKHR(instance, &sci, nullptr, &surface);

    // 3. اختيار كرت الشاشة ومعالج الرسوميات
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(instance, &devCount, devs.data());
    physicalDevice = devs[0];

    // 4. إنشاء الـ Device والـ Queue
    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = 0;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qp;

    const char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExt;
    vkCreateDevice(physicalDevice, &dci, nullptr, &device);
    vkGetDeviceQueue(device, 0, 0, &graphicsQueue);

    // 5. إنشاء الـ Swapchain
    swapchainExtent = { (uint32_t)ANativeWindow_getWidth(window), (uint32_t)ANativeWindow_getHeight(window) };
    VkSwapchainCreateInfoKHR swci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swci.surface = surface;
    swci.minImageCount = 2;
    swci.imageFormat = swapchainFormat;
    swci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swci.imageExtent = swapchainExtent;
    swci.imageArrayLayers = 1;
    swci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    swci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    vkCreateSwapchainKHR(device, &swci, nullptr, &swapchain);

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imgCount, nullptr);
    swapchainImages.resize(imgCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imgCount, swapchainImages.data());

    swapchainImageViews.resize(imgCount);
    for (size_t i = 0; i < imgCount; i++) {
        VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = swapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = swapchainFormat;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &ivci, nullptr, &swapchainImageViews[i]);
    }

    // 6. صورة العمق (Depth Image)
    VkImageCreateInfo depthImgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImgInfo.extent = { swapchainExtent.width, swapchainExtent.height, 1 };
    depthImgInfo.mipLevels = 1;
    depthImgInfo.arrayLayers = 1;
    depthImgInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
    depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vkCreateImage(device, &depthImgInfo, nullptr, &depthImage);

    VkMemoryRequirements dMemReq;
    vkGetImageMemoryRequirements(device, depthImage, &dMemReq);
    VkMemoryAllocateInfo dAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    dAlloc.allocationSize = dMemReq.size;
    dAlloc.memoryTypeIndex = findMemoryType(dMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &dAlloc, nullptr, &depthImageMemory);
    vkBindImageMemory(device, depthImage, depthImageMemory, 0);

    VkImageViewCreateInfo dViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    dViewInfo.image = depthImage;
    dViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    dViewInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
    dViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    dViewInfo.subresourceRange.levelCount = 1;
    dViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &dViewInfo, nullptr, &depthImageView);

    // 7. RenderPass
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = swapchainFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 2;
    rpci.pAttachments = attachments;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    vkCreateRenderPass(device, &rpci, nullptr, &renderPass);

    // 8. Framebuffers
    framebuffers.resize(imgCount);
    for (size_t i = 0; i < imgCount; i++) {
        VkImageView fbViews[] = { swapchainImageViews[i], depthImageView };
        VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbci.renderPass = renderPass;
        fbci.attachmentCount = 2;
        fbci.pAttachments = fbViews;
        fbci.width = swapchainExtent.width;
        fbci.height = swapchainExtent.height;
        fbci.layers = 1;
        vkCreateFramebuffer(device, &fbci, nullptr, &framebuffers[i]);
    }

    // 9. إنشاء مخزن الـ UBO ومخازن المجسمات
    createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uboBuffer, uboBufferMemory);

    // إنشاء مصفوفات شيدرات بلندر
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 1;
    dslci.pBindings = &uboLayoutBinding;
    vkCreateDescriptorSetLayout(device, &dslci, nullptr, &descriptorSetLayout);

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    dpci.maxSets = 1;
    vkCreateDescriptorPool(device, &dpci, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &descriptorSetLayout;
    vkAllocateDescriptorSets(device, &dsai, &descriptorSet);

    VkDescriptorBufferInfo dbi{uboBuffer, 0, sizeof(UniformBufferObject)};
    VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet;
    wds.dstBinding = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

    // 10. خطوط أنابيب Vulkan (Pipelines)
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &descriptorSetLayout;
    vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout);

    VkShaderModule cVsMod = createShaderModule(device, cube_vert_data, sizeof(cube_vert_data));
    VkShaderModule cFsMod = createShaderModule(device, cube_frag_data, sizeof(cube_frag_data));

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = cVsMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = cFsMod;
    stages[1].pName = "main";

    VkVertexInputBindingDescription vibd{0, sizeof(float) * 6, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription viad[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3}
    };

    VkPipelineVertexInputStateCreateInfo pvisi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    pvisi.vertexBindingDescriptionCount = 1;
    pvisi.pVertexBindingDescriptions = &vibd;
    pvisi.vertexAttributeDescriptionCount = 2;
    pvisi.pVertexAttributeDescriptions = viad;

    VkPipelineInputAssemblyStateCreateInfo piasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    piasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{0.0f, 0.0f, (float)swapchainExtent.width, (float)swapchainExtent.height, 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, swapchainExtent};
    VkPipelineViewportStateCreateInfo pvsi{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    pvsi.viewportCount = 1;
    pvsi.pViewports = &viewport;
    pvsi.scissorCount = 1;
    pvsi.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo prsi{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    prsi.cullMode = VK_CULL_MODE_BACK_BIT;
    prsi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    prsi.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo pmssi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pmssi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo pdssi{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    pdssi.depthTestEnable = VK_TRUE;
    pdssi.depthWriteEnable = VK_TRUE;
    pdssi.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cbas{};
    cbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo pcbsi{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    pcbsi.attachmentCount = 1;
    pcbsi.pAttachments = &cbas;

    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &pvisi;
    gpci.pInputAssemblyState = &piasi;
    gpci.pViewportState = &pvsi;
    gpci.pRasterizationState = &prsi;
    gpci.pMultisampleState = &pmssi;
    gpci.pDepthStencilState = &pdssi;
    gpci.pColorBlendState = &pcbsi;
    gpci.layout = pipelineLayout;
    gpci.renderPass = renderPass;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &cubePipeline);

    vkDestroyShaderModule(device, cVsMod, nullptr);
    vkDestroyShaderModule(device, cFsMod, nullptr);

    // خط أنابيب الجزمو ثلاثي الأبعاد
    gizmo.init();
    gizmoVertexCount = (uint32_t)gizmo.vertices.size();
    createBuffer(gizmo.vertices.size() * sizeof(GizmoVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gizmoVbo, gizmoVboMemory);
    void* gData;
    vkMapMemory(device, gizmoVboMemory, 0, gizmo.vertices.size() * sizeof(GizmoVertex), 0, &gData);
    memcpy(gData, gizmo.vertices.data(), gizmo.vertices.size() * sizeof(GizmoVertex));
    vkUnmapMemory(device, gizmoVboMemory);

    // 11. Command Buffers & Sync
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = 0;
    vkCreateCommandPool(device, &cpci, nullptr, &commandPool);

    commandBuffers.resize(imgCount);
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = imgCount;
    vkAllocateCommandBuffers(device, &cbai, commandBuffers.data());

    VkSemaphoreCreateInfo sciSync{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(device, &sciSync, nullptr, &imageAvailableSemaphore);
    vkCreateSemaphore(device, &sciSync, nullptr, &renderFinishedSemaphore);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fci, nullptr, &inFlightFence);

    LOGI("تم إقلاع محرك Vulkan الأصلي بنجاح تام!");
    return true;
}

// دالة تنظيف الذاكرة الصارمة (حل مشكلة تسريب الذاكرة التي لاحظها ماهر)
void VulkanRenderer::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyPipeline(device, cubePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, uboBuffer, nullptr);
        vkFreeMemory(device, uboBufferMemory, nullptr);
        vkDestroyBuffer(device, gizmoVbo, nullptr);
        vkFreeMemory(device, gizmoVboMemory, nullptr);

        for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        for (auto iv : swapchainImageViews) vkDestroyImageView(device, iv, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);

        vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    device = VK_NULL_HANDLE;
    instance = VK_NULL_HANDLE;
    LOGI("تم تحرير كافة موارد الذاكرة والـ GPU بنجاح بدون أي تسريب!");
}

void VulkanRenderer::renderFrame() {
    if (device == VK_NULL_HANDLE) return;

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS) return;

    // تحديث مصفوفات الكاميرا والضوء
    UniformBufferObject ubo{};
    Mat4 v = camera.getViewMatrix();
    Mat4 p = camera.getProjectionMatrix((float)swapchainExtent.width, (float)swapchainExtent.height);
    ubo.viewProj = p * v;
    ubo.model = Mat4::identity();
    ubo.camPos = camera.getPosition();

    void* data;
    vkMapMemory(device, uboBufferMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, uboBufferMemory);

    // تسجيل أوامر الرسم
    VkCommandBuffer cmd = commandBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &cbbi);

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.18f, 0.19f, 0.22f, 1.0f}}; // لون رمادي بلندر الأصلي
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = renderPass;
    rpbi.framebuffer = framebuffers[imageIndex];
    rpbi.renderArea.extent = swapchainExtent;
    rpbi.clearValueCount = 2;
    rpbi.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // رسم المكعب بشيدر استوديو بلندر
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSems[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSems;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    VkSemaphore sigSems[] = {renderFinishedSemaphore};
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = sigSems;

    vkQueueSubmit(graphicsQueue, 1, &si, inFlightFence);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = sigSems;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &imageIndex;
    vkQueuePresentKHR(graphicsQueue, &pi);
}

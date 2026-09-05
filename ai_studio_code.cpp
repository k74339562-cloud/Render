#include "vulkan_renderer.h"
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <android/native_window.h>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

#define LOG_TAG "VulkanEngine"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))

#include "shaders/cube_vert.h"
#include "shaders/cube_frag.h"
#include "shaders/line_vert.h"
#include "shaders/line_frag.h"

struct VertexCube {
    float x, y, z;
    float nx, ny, nz;
};

struct VertexLine {
    float x, y, z;
    float r, g, b, a;
};

static void* g_uboMapped = nullptr;

static VkShaderModule createShaderModule(VkDevice device, const uint32_t* code, size_t sizeBytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = sizeBytes;
    ci.pCode = code;
    VkShaderModule mod = VK_NULL_HANDLE;
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
    if (device != VK_NULL_HANDLE) {
        cleanup();
    }

    const char* ext[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = ext;
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
    qci.queueFamilyIndex = 0;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qp;

    const char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExt;
    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(device, 0, 0, &graphicsQueue);

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

    createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 uboBuffer, uboBufferMemory);
    vkMapMemory(device, uboBufferMemory, 0, sizeof(UniformBufferObject), 0, &g_uboMapped);

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 1;
    dslci.pBindings = &uboBinding;
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

    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &descriptorSetLayout;
    vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout);

    VkViewport viewport{
        0.0f,
        (float)swapchainExtent.height,
        (float)swapchainExtent.width,
        -(float)swapchainExtent.height,
        0.0f,
        1.0f
    };
    VkRect2D scissor{{0, 0}, swapchainExtent};
    VkPipelineViewportStateCreateInfo pvsi{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    pvsi.viewportCount = 1;
    pvsi.pViewports = &viewport;
    pvsi.scissorCount = 1;
    pvsi.pScissors = &scissor;

    // بايبلاين المكعب المصمت
    VkShaderModule cVsMod = createShaderModule(device, cube_vert_data, sizeof(cube_vert_data));
    VkShaderModule cFsMod = createShaderModule(device, cube_frag_data, sizeof(cube_frag_data));

    VkPipelineShaderStageCreateInfo cStages[2] = {};
    cStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    cStages[0].module = cVsMod;
    cStages[0].pName = "main";
    cStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    cStages[1].module = cFsMod;
    cStages[1].pName = "main";

    VkVertexInputBindingDescription cVibd{0, sizeof(VertexCube), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription cViad[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexCube, x)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexCube, nx)}
    };

    VkPipelineVertexInputStateCreateInfo cPvisi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    cPvisi.vertexBindingDescriptionCount = 1;
    cPvisi.pVertexBindingDescriptions = &cVibd;
    cPvisi.vertexAttributeDescriptionCount = 2;
    cPvisi.pVertexAttributeDescriptions = cViad;

    VkPipelineInputAssemblyStateCreateInfo cPiasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    cPiasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

    VkGraphicsPipelineCreateInfo cGpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    cGpci.stageCount = 2;
    cGpci.pStages = cStages;
    cGpci.pVertexInputState = &cPvisi;
    cGpci.pInputAssemblyState = &cPiasi;
    cGpci.pViewportState = &pvsi;
    cGpci.pRasterizationState = &prsi;
    cGpci.pMultisampleState = &pmssi;
    cGpci.pDepthStencilState = &pdssi;
    cGpci.pColorBlendState = &pcbsi;
    cGpci.layout = pipelineLayout;
    cGpci.renderPass = renderPass;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &cGpci, nullptr, &cubePipeline);

    vkDestroyShaderModule(device, cVsMod, nullptr);
    vkDestroyShaderModule(device, cFsMod, nullptr);

    // بايبلاين خطوط شبكة الأرضية
    VkShaderModule lVsMod = createShaderModule(device, line_vert_data, sizeof(line_vert_data));
    VkShaderModule lFsMod = createShaderModule(device, line_frag_data, sizeof(line_frag_data));

    VkPipelineShaderStageCreateInfo lStages[2] = {};
    lStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    lStages[0].module = lVsMod;
    lStages[0].pName = "main";
    lStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    lStages[1].module = lFsMod;
    lStages[1].pName = "main";

    VkVertexInputBindingDescription lVibd{0, sizeof(VertexLine), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription lViad[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexLine, x)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexLine, r)}
    };

    VkPipelineVertexInputStateCreateInfo lPvisi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    lPvisi.vertexBindingDescriptionCount = 1;
    lPvisi.pVertexBindingDescriptions = &lVibd;
    lPvisi.vertexAttributeDescriptionCount = 2;
    lPvisi.pVertexAttributeDescriptions = lViad;

    VkPipelineInputAssemblyStateCreateInfo lPiasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    lPiasi.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineRasterizationStateCreateInfo lPrsi = prsi;
    lPrsi.cullMode = VK_CULL_MODE_NONE;
    lPrsi.lineWidth = 2.0f;

    VkGraphicsPipelineCreateInfo lGpci = cGpci;
    lGpci.pStages = lStages;
    lGpci.pVertexInputState = &lPvisi;
    lGpci.pInputAssemblyState = &lPiasi;
    lGpci.pRasterizationState = &lPrsi;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &lGpci, nullptr, &linePipeline);

    // بايبلاين الجزمو الصلب (مثلثات ثلاثية الأبعاد)
    VkPipelineInputAssemblyStateCreateInfo gPiasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    gPiasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineDepthStencilStateCreateInfo gPdssi = pdssi;
    gPdssi.depthTestEnable = VK_FALSE; // يظهر دائماً في الواجهة فوق المكعب

    VkGraphicsPipelineCreateInfo gGpci = lGpci;
    gGpci.pInputAssemblyState = &gPiasi;
    gGpci.pDepthStencilState = &gPdssi;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gGpci, nullptr, &gizmoPipeline);

    vkDestroyShaderModule(device, lVsMod, nullptr);
    vkDestroyShaderModule(device, lFsMod, nullptr);

    // المكعب
    VertexCube cubeVertices[] = {
        {-1,-1, 1, 0,0,1}, { 1,-1, 1, 0,0,1}, { 1, 1, 1, 0,0,1}, {-1, 1, 1, 0,0,1},
        { 1,-1,-1, 0,0,-1}, {-1,-1,-1, 0,0,-1}, {-1, 1,-1, 0,0,-1}, { 1, 1,-1, 0,0,-1},
        {-1, 1, 1, 0,1,0}, { 1, 1, 1, 0,1,0}, { 1, 1,-1, 0,1,0}, {-1, 1,-1, 0,1,0},
        {-1,-1,-1, 0,-1,0}, { 1,-1,-1, 0,-1,0}, { 1,-1, 1, 0,-1,0}, {-1,-1, 1, 0,-1,0},
        { 1,-1, 1, 1,0,0}, { 1,-1,-1, 1,0,0}, { 1, 1,-1, 1,0,0}, { 1, 1, 1, 1,0,0},
        {-1,-1,-1, -1,0,0}, {-1,-1, 1, -1,0,0}, {-1, 1, 1, -1,0,0}, {-1, 1,-1, -1,0,0}
    };
    uint16_t cubeIndices[] = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
    };

    createBuffer(sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeVbo, cubeVboMemory);
    void* vData;
    vkMapMemory(device, cubeVboMemory, 0, sizeof(cubeVertices), 0, &vData);
    memcpy(vData, cubeVertices, sizeof(cubeVertices));
    vkUnmapMemory(device, cubeVboMemory);

    createBuffer(sizeof(cubeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeIbo, cubeIboMemory);
    void* iData;
    vkMapMemory(device, cubeIboMemory, 0, sizeof(cubeIndices), 0, &iData);
    memcpy(iData, cubeIndices, sizeof(cubeIndices));
    vkUnmapMemory(device, cubeIboMemory);

    // أرضية بلندر
    std::vector<VertexLine> gridLines;
    int gridSize = 12;
    for (int i = -gridSize; i <= gridSize; ++i) {
        float fi = (float)i, fs = (float)gridSize;
        float r = 0.20f, g = 0.22f, b = 0.25f;
        if (i == 0) { r = 0.85f; g = 0.2f; b = 0.2f; } // محور X الأحمر
        gridLines.push_back({-fs, -1.0f, fi, r, g, b, 1.0f});
        gridLines.push_back({ fs, -1.0f, fi, r, g, b, 1.0f});

        r = 0.20f; g = 0.22f; b = 0.25f;
        if (i == 0) { r = 0.2f; g = 0.8f; b = 0.25f; } // محور Z الأخضر
        gridLines.push_back({fi, -1.0f, -fs, r, g, b, 1.0f});
        gridLines.push_back({fi, -1.0f,  fs, r, g, b, 1.0f});
    }
    gridVertexCount = (uint32_t)gridLines.size();

    createBuffer(gridLines.size() * sizeof(VertexLine), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gridVbo, gridVboMemory);
    void* gLineData;
    vkMapMemory(device, gridVboMemory, 0, gridLines.size() * sizeof(VertexLine), 0, &gLineData);
    memcpy(gLineData, gridLines.data(), gridLines.size() * sizeof(VertexLine));
    vkUnmapMemory(device, gridVboMemory);

    // تهيئة الجزمو الصلب (استخدام مصفوفة vertices الموحدة)
    gizmo.init();
    gizmoVertexCount = (uint32_t)gizmo.vertices.size();
    createBuffer(gizmo.vertices.size() * sizeof(GizmoVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, gizmoVbo, gizmoVboMemory);
    void* gizmoData;
    vkMapMemory(device, gizmoVboMemory, 0, gizmo.vertices.size() * sizeof(GizmoVertex), 0, &gizmoData);
    memcpy(gizmoData, gizmo.vertices.data(), gizmo.vertices.size() * sizeof(GizmoVertex));
    vkUnmapMemory(device, gizmoVboMemory);

    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = 0;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

    LOGI("نجح إقلاع محرك Vulkan بعد معالجة تعارض الجزمو!");
    return true;
}

void VulkanRenderer::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        if (g_uboMapped) {
            vkUnmapMemory(device, uboBufferMemory);
            g_uboMapped = nullptr;
        }

        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyPipeline(device, gizmoPipeline, nullptr);
        vkDestroyPipeline(device, linePipeline, nullptr);
        vkDestroyPipeline(device, cubePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, gizmoVbo, nullptr);
        vkFreeMemory(device, gizmoVboMemory, nullptr);

        vkDestroyBuffer(device, gridVbo, nullptr);
        vkFreeMemory(device, gridVboMemory, nullptr);

        vkDestroyBuffer(device, cubeIbo, nullptr);
        vkFreeMemory(device, cubeIboMemory, nullptr);
        vkDestroyBuffer(device, cubeVbo, nullptr);
        vkFreeMemory(device, cubeVboMemory, nullptr);

        vkDestroyBuffer(device, uboBuffer, nullptr);
        vkFreeMemory(device, uboBufferMemory, nullptr);

        for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        framebuffers.clear();

        vkDestroyRenderPass(device, renderPass, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        for (auto iv : swapchainImageViews) vkDestroyImageView(device, iv, nullptr);
        swapchainImageViews.clear();

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
    }

    if (instance != VK_NULL_HANDLE) {
        if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    device = VK_NULL_HANDLE;
    instance = VK_NULL_HANDLE;
    surface = VK_NULL_HANDLE;
}

void VulkanRenderer::renderFrame() {
    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE) return;

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS) return;

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

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.19f, 0.20f, 0.23f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = renderPass;
    rpbi.framebuffer = framebuffers[imageIndex];
    rpbi.renderArea.extent = swapchainExtent;
    rpbi.clearValueCount = 2;
    rpbi.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkDeviceSize offsets[] = {0};

    // 1. رسم شبكة أرضية بلندر
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &gridVbo, offsets);
    vkCmdDraw(cmd, gridVertexCount, 1, 0, 0);

    // 2. رسم المكعب المصمت بشيدر بلندر
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVbo, offsets);
    vkCmdBindIndexBuffer(cmd, cubeIbo, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

    // 3. رسم الجزمو الصلب بالكامل (أعمدة صلبة + مخاريط)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gizmoPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &gizmoVbo, offsets);
    vkCmdDraw(cmd, gizmoVertexCount, 1, 0, 0);

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
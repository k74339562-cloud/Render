#include "vulkan_pipeline.h"
#include <cstddef>
#include "shaders/cube_vert.h"
#include "shaders/cube_frag.h"
#include "shaders/line_vert.h"
#include "shaders/line_frag.h"

struct VertexCube { float x, y, z; float nx, ny, nz; };
struct VertexLine { float x, y, z; float r, g, b, a; };

static VkShaderModule createShaderModule(VkDevice device, const uint32_t* code, size_t sizeBytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = sizeBytes;
    ci.pCode = code;
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}

uint32_t VulkanPipeline::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

VkFormat VulkanPipeline::findSupportedDepthFormat(VkPhysicalDevice physicalDevice) {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    return VK_FORMAT_D16_UNORM;
}

bool VulkanPipeline::init(VkDevice device, VkPhysicalDevice physicalDevice, VkFormat swapchainFormat, 
                          VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews, VkBuffer uboBuffer) {
    depthFormat = findSupportedDepthFormat(physicalDevice);

    // 1. صورة العمق
    VkImageCreateInfo depthImgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImgInfo.extent = { extent.width, extent.height, 1 };
    depthImgInfo.mipLevels = 1;
    depthImgInfo.arrayLayers = 1;
    depthImgInfo.format = depthFormat;
    depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vkCreateImage(device, &depthImgInfo, nullptr, &depthImage);

    VkMemoryRequirements dMemReq;
    vkGetImageMemoryRequirements(device, depthImage, &dMemReq);
    VkMemoryAllocateInfo dAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    dAlloc.allocationSize = dMemReq.size;
    dAlloc.memoryTypeIndex = findMemoryType(physicalDevice, dMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device, &dAlloc, nullptr, &depthImageMemory);
    vkBindImageMemory(device, depthImage, depthImageMemory, 0);

    VkImageViewCreateInfo dViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    dViewInfo.image = depthImage;
    dViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    dViewInfo.format = depthFormat;
    dViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    dViewInfo.subresourceRange.levelCount = 1;
    dViewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &dViewInfo, nullptr, &depthImageView);

    // 2. RenderPass
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = swapchainFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = depthFormat;
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

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 2;
    rpci.pAttachments = attachments;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dependency;
    vkCreateRenderPass(device, &rpci, nullptr, &renderPass);

    // 3. Framebuffers
    framebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkImageView fbViews[] = { swapchainImageViews[i], depthImageView };
        VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbci.renderPass = renderPass;
        fbci.attachmentCount = 2;
        fbci.pAttachments = fbViews;
        fbci.width = extent.width;
        fbci.height = extent.height;
        fbci.layers = 1;
        vkCreateFramebuffer(device, &fbci, nullptr, &framebuffers[i]);
    }

    // 4. Descriptors
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

    VkDescriptorBufferInfo dbi{uboBuffer, 0, 144};
    VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet;
    wds.dstBinding = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

    // 5. إعداد Pipelines
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &descriptorSetLayout;
    vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout);

    VkViewport viewport{ 0.0f, (float)extent.height, (float)extent.width, -(float)extent.height, 0.0f, 1.0f };
    VkRect2D scissor{{0, 0}, extent};
    VkPipelineViewportStateCreateInfo pvsi{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    pvsi.viewportCount = 1;
    pvsi.pViewports = &viewport;
    pvsi.scissorCount = 1;
    pvsi.pScissors = &scissor;

    // (أ) شيدر المكعب
    VkShaderModule cVs = createShaderModule(device, cube_vert_data, sizeof(cube_vert_data));
    VkShaderModule cFs = createShaderModule(device, cube_frag_data, sizeof(cube_frag_data));

    VkPipelineShaderStageCreateInfo cStages[2] = {};
    cStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; cStages[0].module = cVs; cStages[0].pName = "main";
    cStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; cStages[1].module = cFs; cStages[1].pName = "main";

    VkVertexInputBindingDescription cVibd{0, sizeof(VertexCube), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription cViad[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexCube, x)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexCube, nx)}
    };

    VkPipelineVertexInputStateCreateInfo cPvisi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    cPvisi.vertexBindingDescriptionCount = 1; cPvisi.pVertexBindingDescriptions = &cVibd;
    cPvisi.vertexAttributeDescriptionCount = 2; cPvisi.pVertexAttributeDescriptions = cViad;

    VkPipelineInputAssemblyStateCreateInfo cPiasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    cPiasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo prsi{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    prsi.cullMode = VK_CULL_MODE_BACK_BIT;
    // تصحيح Winding Order: لأن الـ Viewport سالب، الأوجه الخارجية تصبح CLOCKWISE
    prsi.frontFace = VK_FRONT_FACE_CLOCKWISE;
    prsi.lineWidth = 1.0f;
    prsi.depthBiasEnable = VK_TRUE;
    prsi.depthBiasConstantFactor = 2.0f;
    prsi.depthBiasSlopeFactor = 2.0f;

    VkPipelineMultisampleStateCreateInfo pmssi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pmssi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo pdssi{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    pdssi.depthTestEnable = VK_TRUE; pdssi.depthWriteEnable = VK_TRUE; pdssi.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState cbas{};
    cbas.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo pcbsi{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    pcbsi.attachmentCount = 1; pcbsi.pAttachments = &cbas;

    VkGraphicsPipelineCreateInfo cGpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    cGpci.stageCount = 2; cGpci.pStages = cStages;
    cGpci.pVertexInputState = &cPvisi; cGpci.pInputAssemblyState = &cPiasi;
    cGpci.pViewportState = &pvsi; cGpci.pRasterizationState = &prsi;
    cGpci.pMultisampleState = &pmssi; cGpci.pDepthStencilState = &pdssi;
    cGpci.pColorBlendState = &pcbsi; cGpci.layout = pipelineLayout; cGpci.renderPass = renderPass;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &cGpci, nullptr, &cubePipeline);

    vkDestroyShaderModule(device, cVs, nullptr);
    vkDestroyShaderModule(device, cFs, nullptr);

    // (ب) شيدر خطوط شبكة الأرضية وحواف المكعب مع تفعيل الدمج والشفافية (Alpha Blending)
    VkShaderModule lVs = createShaderModule(device, line_vert_data, sizeof(line_vert_data));
    VkShaderModule lFs = createShaderModule(device, line_frag_data, sizeof(line_frag_data));

    VkPipelineShaderStageCreateInfo lStages[2] = {};
    lStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; lStages[0].module = lVs; lStages[0].pName = "main";
    lStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; lStages[1].module = lFs; lStages[1].pName = "main";

    VkVertexInputBindingDescription lVibd{0, sizeof(VertexLine), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription lViad[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexLine, x)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexLine, r)}
    };

    VkPipelineVertexInputStateCreateInfo lPvisi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    lPvisi.vertexBindingDescriptionCount = 1; lPvisi.pVertexBindingDescriptions = &lVibd;
    lPvisi.vertexAttributeDescriptionCount = 2; lPvisi.pVertexAttributeDescriptions = lViad;

    VkPipelineInputAssemblyStateCreateInfo lPiasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    lPiasi.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineRasterizationStateCreateInfo lPrsi = prsi;
    lPrsi.depthBiasEnable = VK_FALSE;
    lPrsi.cullMode = VK_CULL_MODE_NONE;
    lPrsi.lineWidth = 1.0f;

    // تفعيل Alpha Blending للخطوط لتتلاشى بانسيابية فائقة
    VkPipelineColorBlendAttachmentState lbas{};
    lbas.colorWriteMask = 0xF;
    lbas.blendEnable = VK_TRUE;
    lbas.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    lbas.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    lbas.colorBlendOp = VK_BLEND_OP_ADD;
    lbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    lbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    lbas.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo lpcbsi{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    lpcbsi.attachmentCount = 1; lpcbsi.pAttachments = &lbas;

    VkGraphicsPipelineCreateInfo lGpci = cGpci;
    lGpci.pStages = lStages; lGpci.pVertexInputState = &lPvisi;
    lGpci.pInputAssemblyState = &lPiasi; lGpci.pRasterizationState = &lPrsi;
    lGpci.pColorBlendState = &lpcbsi;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &lGpci, nullptr, &linePipeline);

    // (ج) شيدر الجزمو
    VkPipelineInputAssemblyStateCreateInfo gPiasi{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    gPiasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineDepthStencilStateCreateInfo gPdssi = pdssi;
    gPdssi.depthTestEnable = VK_TRUE;
    gPdssi.depthWriteEnable = VK_TRUE;
    gPdssi.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkGraphicsPipelineCreateInfo gGpci = lGpci;
    gGpci.pInputAssemblyState = &gPiasi; gGpci.pDepthStencilState = &gPdssi;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gGpci, nullptr, &gizmoPipeline);

    vkDestroyShaderModule(device, lVs, nullptr);
    vkDestroyShaderModule(device, lFs, nullptr);

    return true;
}

void VulkanPipeline::cleanup(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;

    vkDestroyPipeline(device, gizmoPipeline, nullptr);
    vkDestroyPipeline(device, linePipeline, nullptr);
    vkDestroyPipeline(device, cubePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers.clear();

    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);
}

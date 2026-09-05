#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanPipeline {
public:
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    VkPipeline cubePipeline = VK_NULL_HANDLE;
    VkPipeline linePipeline = VK_NULL_HANDLE;
    VkPipeline gizmoPipeline = VK_NULL_HANDLE;

    bool init(VkDevice device, VkPhysicalDevice physicalDevice, VkFormat swapchainFormat, 
              VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews, VkBuffer uboBuffer);
    void cleanup(VkDevice device);

private:
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

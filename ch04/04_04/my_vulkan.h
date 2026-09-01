#ifndef MY_VULKAN_H
#define MY_VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

/* NOTES
 *
 * vk_<fname>() - (with underscore) are my vulkan setup functions separate from
 * vk<Function> - which are functions from the vulkan library
 * my_<fname>() - orchestration layer that handles error mapping and messaging
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkDevice m_device;
extern VkQueue m_queue;
extern uint32_t m_queueFamilyIndex;

struct vk_image
{
    VkImage handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkExtent3D extent = {0, 0, 1};
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};
extern std::vector<vk_image> m_images;

struct vk_command_buffer
{
    VkCommandBuffer handle = VK_NULL_HANDLE;
};
extern std::vector<vk_command_buffer> m_commandBuffers;

extern VkCommandPool m_commandPool;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

// setup functions
VkResult vk_init_instance(void);
VkResult vk_get_physical_device(void);
VkResult vk_get_queue_family(void);
VkResult vk_create_logical_device(void);
VkResult vk_create_command_pool(void);
VkResult vk_create_command_buffer(vk_command_buffer *outCmdBuf);

// resource creation
VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format,
                         VkImageUsageFlags usage, VkSampleCountFlagBits samples,
                         vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);

// image operations - resolve and blit
VkResult vk_record_resolve_image(VkCommandBuffer cmdBuf, VkImage srcImage,
                                 VkImageLayout srcLayout, VkImage dstImage,
                                 VkImageLayout dstLayout, uint32_t width, uint32_t height);
VkResult vk_record_blit_image(VkCommandBuffer cmdBuf, VkImage srcImage,
                              VkImageLayout srcLayout, uint32_t srcWidth, uint32_t srcHeight,
                              VkImage dstImage, VkImageLayout dstLayout,
                              uint32_t dstWidth, uint32_t dstHeight);

// cleanup
VkResult vk_cleanup(void);

// orchestration wrappers
void my_init_vulkan(void);
void my_resolve_and_blit_image(void);

#endif // MY_VULKAN_H

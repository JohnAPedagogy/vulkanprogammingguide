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
                         VkImageUsageFlags usage, vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);

// barrier operations
VkResult vk_record_barrier(VkCommandBuffer cmdBuf, VkImage image,
                           VkImageLayout oldLayout, VkImageLayout newLayout,
                           VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                           VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

// cleanup
VkResult vk_cleanup(void);

// orchestration wrappers
void my_init_vulkan(void);
void my_create_image_and_barrier(void);

// verbose debug functions
void dbg_print_image_info(const vk_image *img);

#endif // MY_VULKAN_H

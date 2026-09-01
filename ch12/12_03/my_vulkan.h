#ifndef MY_VULKAN_H
#define MY_VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

/* NOTES
 *
 * vk_<fname>() - (with underscore) are wrapper functions around vulkan calls
 * vk<Function> - functions from the vulkan library
 * my_<fname>() - orchestration layer that calls vk_*() and prints results
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkDevice m_device;
extern VkQueue m_graphicsQueue;
extern VkCommandPool m_commandPool;
extern VkCommandBuffer m_commandBuffer;

struct vk_buffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};
extern std::vector<vk_buffer> m_buffers;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

// vk_* functions
VkResult vk_init_instance();
VkResult vk_init_device();
VkResult vk_init_queues();
VkResult vk_init_command_pool();
VkResult vk_allocate_command_buffer(VkCommandBuffer *outCmd);
VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer);
VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties);
VkResult vk_cleanup();

// my_* functions
void my_init_vulkan(void);
void my_create_source_buffer(VkDeviceSize size);
void my_create_staging_buffer(VkDeviceSize size);
void my_copy_and_readback(VkDeviceSize size);

// dbg_* functions
void dbg_buffer_info(const vk_buffer *buffer);

#endif // MY_VULKAN_H

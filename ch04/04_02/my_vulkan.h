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
extern uint32_t m_queueFamilyIndex;

struct vk_buffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};
extern std::vector<vk_buffer> m_buffers;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

// setup functions
VkResult vk_init_instance(void);
VkResult vk_get_physical_device(void);
VkResult vk_get_queue_family(void);
VkResult vk_create_logical_device(void);

// resource creation
VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer);
VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties);

// buffer operations
VkResult vk_update_buffer(VkDeviceMemory memory, VkDeviceSize offset,
                          VkDeviceSize size, const void *data);
VkResult vk_readback_buffer(VkDeviceMemory memory, VkDeviceSize offset,
                            VkDeviceSize size, void *outData);

// cleanup
VkResult vk_cleanup(void);

// orchestration wrappers
void my_init_vulkan(void);
void my_update_buffer_and_readback(void);

#endif // MY_VULKAN_H

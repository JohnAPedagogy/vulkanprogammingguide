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
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern VkDebugUtilsMessengerEXT m_debugMessenger;
extern int device_count;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);

VkResult vk_device_init_count(int *count);
VkResult vk_create_debug_messenger(void);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_cleanup();
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_get_layer_properties(int device_index, uint32_t *numInstanceLayers);
VkResult vk_get_extensions(uint32_t *numInstanceExtensions);
VkResult vk_create_command_pool(VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex, VkCommandPool *outPool);
VkResult vk_allocate_command_buffers(VkCommandPool pool, uint32_t count, VkCommandBuffer *outBuffers);
VkResult vk_destroy_command_pool(VkCommandPool pool);

void my_init_vulkan(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_get_layer_properties(int deviceIndex);
void my_get_extensions(void);
void my_allocate_pool_batch(uint32_t queueFamilyIndex);

// verbose debug functions
void dbg_show_layer_property_names(VkLayerProperties* p, int count);

#endif // MY_VULKAN_H

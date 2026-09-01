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
extern VkQueue m_graphicsQueue;
extern VkCommandPool m_commandPool;
extern VkCommandBuffer m_commandBuffer;
extern int device_count;

struct vk_image
{
    VkImage handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkExtent3D extent = {};
    VkFormat format = VK_FORMAT_UNDEFINED;
};
extern std::vector<vk_image> m_images;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);

VkResult vk_device_init_count(int *count);
VkResult vk_create_debug_messenger(void);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_cleanup();
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_get_layer_properties(int device_index, uint32_t *numInstanceLayers);
VkResult vk_get_extensions(uint32_t *numInstanceExtensions);
VkResult vk_create_image(VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);
VkResult vk_get_device_queue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *outQueue);
VkResult vk_create_command_pool(uint32_t queueFamilyIndex, VkCommandPool *outPool);
VkResult vk_allocate_command_buffer(VkCommandPool pool, VkCommandBuffer *outBuffer);
VkResult vk_record_script(VkCommandBuffer commandBuffer, VkImage image);
VkResult vk_submit_and_wait(VkQueue queue, VkCommandBuffer commandBuffer);

void my_init_vulkan(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_get_layer_properties(int deviceIndex);
void my_get_extensions(void);
void my_create_image(void);
void my_get_device_queue(uint32_t queueFamilyIndex, uint32_t queueIndex);
void my_create_command_pool(uint32_t queueFamilyIndex);
void my_allocate_command_buffer(void);
void my_record_script(void);
void my_submit_and_wait(void);

// verbose debug functions
void dbg_show_layer_property_names(VkLayerProperties* p, int count);

#endif // MY_VULKAN_H

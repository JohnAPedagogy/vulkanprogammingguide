#ifndef MY_VULKAN_H
#define MY_VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

/* NOTES
 *
 * vk_<fname>() - (with underscore) are my vulkan setup functions separate from
 * vk<Function> - which are functions from the vulkan library
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern int device_count;

VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_cleanup();
VkResult vk_get_logical_device(int device_index, int *feature_count);

// Validation layer support
bool check_validation_layer_support();
std::vector<const char*> get_required_validation_layers();
std::vector<const char*> get_instance_extensions();

void my_init_vulkan();
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);

#endif // MY_VULKAN_H

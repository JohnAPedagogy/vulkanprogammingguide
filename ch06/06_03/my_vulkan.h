#ifndef MY_VULKAN_H
#define MY_VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern int device_count;

struct vk_shader_module
{
    VkShaderModule handle = VK_NULL_HANDLE;
};
extern std::vector<vk_shader_module> m_shaderModules;

size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);

VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_cleanup();
VkResult vk_create_shader_module(const uint32_t *code, size_t codeSize, vk_shader_module *outModule);
VkResult vk_track_shader_module(vk_shader_module *module);

void my_init_vulkan(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_create_shader_with_specialization(void);

void dbg_show_layer_property_names(VkLayerProperties* p, int count);

#endif // MY_VULKAN_H

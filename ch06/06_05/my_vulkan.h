#ifndef MY_VULKAN_H
#define MY_VULKAN_H
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern int device_count;
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_cleanup();
VkResult vk_create_descriptor_set_layout(VkDescriptorSetLayout *outLayout);
VkResult vk_update_descriptor_set(VkDescriptorSet set, uint32_t binding);
void my_init_vulkan(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_declare_resources(void);
#endif

#ifndef MY_VULKAN_H
#define MY_VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <ctime>

/* NOTES
 *
 * vk_<fname>() - (with underscore) are my vulkan setup functions separate from
 * vk<Function> - which are functions from the vulkan library
 *
 * my_<fname>() - orchestration layer that turns vk_* results into messages
 *
 * Three prefixes:
 * - vk<Function>() - real Vulkan API
 * - vk_<name>() - project's own wrapper functions, returns VkResult
 * - my_<name>() - thin orchestration layer that calls vk_* and prints messages
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern VkQueue m_queue;
extern int device_count;

struct vk_fence
{
    VkFence handle = VK_NULL_HANDLE;
};
extern std::vector<vk_fence> m_fences;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);

// vk_* functions
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_create_fence(vk_fence *outFence);
VkResult vk_create_command_buffer(VkCommandBuffer *outCmdBuffer);
VkResult vk_cleanup();

// my_* functions
void my_init_vulkan(void);
void my_run_fence_test(void);

#endif // MY_VULKAN_H

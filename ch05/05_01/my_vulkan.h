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
extern uint32_t m_queueFamilyIndex;
extern GLFWwindow *m_window;
extern VkSurfaceKHR m_surface;
extern int device_count;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *features);

VkResult vk_create_window(int width, int height, const char *title, GLFWwindow **outWindow);
VkResult vk_device_init_count(int *count);
VkResult vk_create_debug_messenger(void);
VkResult vk_create_surface(GLFWwindow *window, VkSurfaceKHR *outSurface);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_get_present_support(int deviceIndex, VkBool32 *outSupported);
VkResult vk_cleanup(void);

void my_init_vulkan(void);
void my_create_window(void);
void my_create_surface(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_check_present_support(int deviceIndex);

#endif // MY_VULKAN_H

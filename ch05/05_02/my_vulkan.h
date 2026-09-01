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
 *
 * Three naming layers:
 * - vk<Function>() — the real Vulkan API
 * - vk_<name>() — this project's setup/wrapper functions (returns VkResult)
 * - my_<name>() — orchestration layer that calls vk_* and turns VkResult into console messages
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern GLFWwindow *m_window;
extern VkSurfaceKHR m_surface;
extern VkSwapchainKHR m_swapchain;
extern std::vector<VkImage> m_swapchainImages;
extern int device_count;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *features);

// vk_* functions (setup/wrapper layer, returns VkResult)
VkResult vk_create_window(int width, int height, const char *title, GLFWwindow **outWindow);
VkResult vk_create_surface(GLFWwindow *window, VkSurfaceKHR *outSurface);
VkResult vk_create_swapchain(uint32_t width, uint32_t height, VkSwapchainKHR *outSwapchain);
VkResult vk_get_swapchain_images(VkSwapchainKHR swapchain);
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_get_present_support(int deviceIndex, VkBool32 *outSupported);
VkResult vk_cleanup(void);

// my_* functions (orchestration layer, prints messages)
void my_init_vulkan(void);
void my_create_window(void);
void my_create_surface(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_check_present_support(int deviceIndex);
void my_create_swapchain(uint32_t width, uint32_t height);
void my_get_swapchain_images(void);

#endif // MY_VULKAN_H

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
extern int device_count;

VkResult vk_device_init_count(int *count);
VkResult vk_cleanup();

void my_init_vulkan();


#endif // MY_VULKAN_H

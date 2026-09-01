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
extern int device_count;

struct vk_image
{
    VkImage handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct vk_image_view
{
    VkImageView handle = VK_NULL_HANDLE;
};

struct vk_render_pass
{
    VkRenderPass handle = VK_NULL_HANDLE;
};

struct vk_framebuffer
{
    VkFramebuffer handle = VK_NULL_HANDLE;
};

extern std::vector<vk_image> m_images;
extern std::vector<vk_image_view> m_imageViews;
extern std::vector<vk_render_pass> m_renderPasses;
extern std::vector<vk_framebuffer> m_framebuffers;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);

VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_cleanup();
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_get_layer_properties(int device_index, uint32_t *numInstanceLayers);
VkResult vk_get_extensions(uint32_t *numInstanceExtensions);
VkResult vk_create_render_pass(VkFormat colorFormat, VkRenderPass *outRenderPass);
VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);
VkResult vk_create_image_view(VkImage imageHandle, VkFormat format, VkImageView *outImageView);
VkResult vk_create_framebuffer(VkRenderPass renderPass, VkImageView colorView, uint32_t width, uint32_t height, VkFramebuffer *outFramebuffer);

void my_init_vulkan(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_get_layer_properties(int deviceIndex);
void my_get_extensions(void);
void my_create_framebuffer_setup(void);

// verbose debug functions
void dbg_show_layer_property_names(VkLayerProperties* p, int count);

#endif // MY_VULKAN_H

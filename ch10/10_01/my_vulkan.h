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
extern VkQueue m_queue;
extern VkCommandPool m_commandPool;
extern VkCommandBuffer m_commandBuffer;
extern int device_count;

struct vk_image
{
    VkImage handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

struct vk_render_target
{
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    std::vector<vk_image> attachments;
};

extern std::vector<vk_render_target> m_renderTargets;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

// vk_* functions
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format,
                         VkImageUsageFlags usage, vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);
VkResult vk_create_image_view(vk_image *image);
VkResult vk_create_render_pass(uint32_t colorAttachmentCount, const VkFormat *formats,
                               VkRenderPass *outRenderPass);
VkResult vk_create_framebuffer(VkRenderPass renderPass, uint32_t width, uint32_t height,
                               uint32_t attachmentCount, const VkImageView *views,
                               VkFramebuffer *outFramebuffer);
VkResult vk_create_command_pool(VkCommandPool *outPool);
VkResult vk_allocate_command_buffer(VkCommandPool pool, VkCommandBuffer *outBuffer);
VkResult vk_cleanup();

// my_* functions
void my_init_vulkan(void);
void my_get_logical_device(int device_index);
void my_create_render_target(void);

#endif // MY_VULKAN_H

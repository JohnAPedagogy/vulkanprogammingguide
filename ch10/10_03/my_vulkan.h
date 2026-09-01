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
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};

struct vk_msaa_target
{
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    vk_image msaaImage;
    vk_image resolveImage;
};

extern std::vector<vk_msaa_target> m_msaaTargets;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

// vk_* functions
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format,
                         VkImageUsageFlags usage, VkSampleCountFlagBits samples, vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);
VkResult vk_create_image_view(vk_image *image);
VkResult vk_create_msaa_render_pass(VkRenderPass *outRenderPass);
VkResult vk_create_msaa_framebuffer(VkRenderPass renderPass, uint32_t width, uint32_t height,
                                    VkImageView msaaView, VkImageView resolveView,
                                    VkFramebuffer *outFramebuffer);
VkResult vk_create_command_pool(VkCommandPool *outPool);
VkResult vk_allocate_command_buffer(VkCommandPool pool, VkCommandBuffer *outBuffer);
VkResult vk_cleanup();

// my_* functions
void my_init_vulkan(void);
void my_get_logical_device(int device_index);
void my_create_msaa_target(void);

#endif // MY_VULKAN_H

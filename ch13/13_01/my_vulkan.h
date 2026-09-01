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
 * Naming layers:
 * - vk<Function>()    the real Vulkan API (never redefine, only call)
 * - vk_<name>()       this project's own setup/wrapper functions (returns VkResult)
 * - my_<name>()       orchestration layer that calls vk_* and prints messages
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern VkQueue m_queue;
extern int device_count;

struct vk_buffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};
extern std::vector<vk_buffer> m_buffers;

struct vk_image
{
    VkImage handle = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkExtent2D extent;
    VkFormat format;
};
extern std::vector<vk_image> m_images;

struct vk_renderpass
{
    VkRenderPass handle = VK_NULL_HANDLE;
};
extern vk_renderpass m_renderPass;

struct vk_framebuffer
{
    VkFramebuffer handle = VK_NULL_HANDLE;
};
extern vk_framebuffer m_framebuffer;

struct vk_pipeline
{
    VkPipeline handle = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};
extern std::vector<vk_pipeline> m_pipelines;

struct vk_shader_module
{
    VkShaderModule handle = VK_NULL_HANDLE;
};
extern std::vector<vk_shader_module> m_shaderModules;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);
VkResult vk_load_spv(const char *spvPath, std::vector<uint32_t> *outCode);

// vk_* functions - setup/wrapper functions
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_cleanup();
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer);
VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties);
VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage);
VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties);
VkResult vk_create_shader_module(const char *spvPath, vk_shader_module *outModule);
VkResult vk_create_renderpass_with_input_attachments(VkRenderPass *outRenderPass);
VkResult vk_create_framebuffer(VkRenderPass renderPass, VkFramebuffer *outFramebuffer);
VkResult vk_create_graphics_pipeline(VkShaderModule vertModule, VkShaderModule fragModule,
    VkRenderPass renderPass, uint32_t subpassIndex, vk_pipeline *outPipeline);

// my_* functions - orchestration layer
void my_init_vulkan(void);
void my_get_device_properties(int deviceIndex);
void my_get_logical_device(int deviceIndex);
void my_create_images(void);
void my_create_shader_modules(void);
void my_create_renderpass(void);
void my_create_framebuffer(void);
void my_create_pipelines(void);
void my_run_input_attachment_pass(void);

// verbose debug functions
void dbg_show_layer_property_names(VkLayerProperties* p, int count);

#endif // MY_VULKAN_H

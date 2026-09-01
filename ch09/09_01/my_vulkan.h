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
 * My functions use:
 *  - vk_*() for the low-level Vulkan API wrappers returning VkResult
 *  - my_*() for the orchestration layer that calls vk_* and prints messages
 *  - dbg_*() for verbose debug output
 */

extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkDevice m_device;
extern VkQueue m_queue;
extern uint32_t m_queueFamilyIndex;

struct vk_shader_module
{
    VkShaderModule handle = VK_NULL_HANDLE;
};
extern std::vector<vk_shader_module> m_shaderModules;

struct vk_render_pass
{
    VkRenderPass handle = VK_NULL_HANDLE;
};
extern vk_render_pass m_renderPass;

struct vk_framebuffer
{
    VkFramebuffer handle = VK_NULL_HANDLE;
};
extern vk_framebuffer m_framebuffer;

struct vk_pipeline
{
    VkPipeline handle = VK_NULL_HANDLE;
};
extern vk_pipeline m_pipeline;

struct vk_command_buffer
{
    VkCommandBuffer handle = VK_NULL_HANDLE;
};
extern vk_command_buffer m_commandBuffer;

struct vk_image
{
    VkImage handle = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};
extern vk_image m_outputImage;

// helper functions
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

// vk_* functions - low-level Vulkan API wrappers
VkResult vk_init_instance();
VkResult vk_get_physical_device();
VkResult vk_create_device();
VkResult vk_get_queue();
VkResult vk_create_command_pool_and_buffer();
VkResult vk_create_shader_module(const char *spvPath, vk_shader_module *outModule);
VkResult vk_create_render_pass();
VkResult vk_create_output_image();
VkResult vk_create_framebuffer();
VkResult vk_create_tessellation_pipeline();
VkResult vk_cleanup();

// my_* functions - orchestration with message output
void my_init_vulkan();
void my_create_tessellation_pipeline();
void my_run_tessellation();

// dbg_* functions - debug output
void dbg_print_instance_extensions();

#endif // MY_VULKAN_H

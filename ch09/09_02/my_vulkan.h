#ifndef MY_VULKAN_H
#define MY_VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <glm/glm.hpp>

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

struct vk_buffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};
extern std::vector<vk_buffer> m_buffers;

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
extern vk_image m_heightMap;

struct vk_sampler
{
    VkSampler handle = VK_NULL_HANDLE;
};
extern vk_sampler m_sampler;

struct vk_descriptor_set_layout
{
    VkDescriptorSetLayout handle = VK_NULL_HANDLE;
};
extern vk_descriptor_set_layout m_descriptorSetLayout;

struct vk_descriptor_set
{
    VkDescriptorSet handle = VK_NULL_HANDLE;
};
extern vk_descriptor_set m_descriptorSet;

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
VkResult vk_create_height_map_image();
VkResult vk_create_sampler();
VkResult vk_create_descriptor_set_layout();
VkResult vk_create_descriptor_pool();
VkResult vk_create_descriptor_set();
VkResult vk_create_camera_buffer(vk_buffer *outBuffer);
VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties);
VkResult vk_create_framebuffer();
VkResult vk_create_displacement_pipeline();
VkResult vk_cleanup();

// my_* functions - orchestration with message output
void my_init_vulkan();
void my_create_displacement_pipeline();
void my_run_displacement();

// dbg_* functions - debug output
void dbg_print_instance_extensions();

#endif // MY_VULKAN_H

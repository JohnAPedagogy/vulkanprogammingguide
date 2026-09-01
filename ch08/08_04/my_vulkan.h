#ifndef MY_VULKAN_H
#define MY_VULKAN_H
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <stdint.h>
extern VkInstance m_instance;
extern VkPhysicalDevice m_physicalDevice;
extern VkPhysicalDevice *m_devices;
extern VkDevice m_device;
extern VkQueue m_queue;
extern int device_count;
struct vk_image { VkImage handle = VK_NULL_HANDLE; VkDeviceMemory memory = VK_NULL_HANDLE; VkFormat format = VK_FORMAT_UNDEFINED; uint32_t width = 0; uint32_t height = 0; };
extern vk_image m_colorImage;
struct vk_render_pass { VkRenderPass handle = VK_NULL_HANDLE; };
extern vk_render_pass m_renderPass;
struct vk_framebuffer { VkFramebuffer handle = VK_NULL_HANDLE; };
extern vk_framebuffer m_framebuffer;
struct vk_pipeline { VkPipeline handle = VK_NULL_HANDLE; VkPipelineLayout layout = VK_NULL_HANDLE; };
extern vk_pipeline m_pipeline;
struct vk_buffer { VkBuffer handle = VK_NULL_HANDLE; VkDeviceMemory memory = VK_NULL_HANDLE; VkDeviceSize size = 0; };
extern std::vector<vk_buffer> m_buffers;
size_t count_enabled_features(const VkPhysicalDeviceFeatures *device);
VkResult vk_device_init_count(int *count);
VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount);
VkResult vk_get_logical_device(int device_index, int *feature_count);
VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage);
VkResult vk_track_image(vk_image *image, uint32_t width, uint32_t height, VkFormat format, VkMemoryPropertyFlags properties);
VkResult vk_create_shader_module(const std::vector<char> *spvCode, VkShaderModule *outModule);
VkResult vk_create_render_pass(VkFormat colorFormat, vk_render_pass *outRenderPass);
VkResult vk_create_framebuffer(vk_image *colorImage, vk_render_pass *renderPass, vk_framebuffer *outFramebuffer);
VkResult vk_create_pipeline(VkShaderModule vertShader, VkShaderModule fragShader, vk_render_pass *renderPass, vk_pipeline *outPipeline);
VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer);
VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties);
VkResult vk_cleanup();
void my_init_vulkan(void);
void my_create_image(void);
void my_create_shader_modules(VkShaderModule *vertShader, VkShaderModule *fragShader);
void my_create_render_pass(void);
void my_create_framebuffer(void);
void my_create_pipeline(VkShaderModule vertShader, VkShaderModule fragShader);
void my_create_vertex_and_indirect_buffers(void);
void my_record_and_submit_indirect_draw(void);
#endif

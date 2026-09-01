#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Chapter 8.1: Issuing a Draw\n";
    std::cout << "============================\n\n";

    my_init_vulkan();
    if (device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 1;
    }

    uint32_t queueFamilyPropertyCount = 0;
    vk_get_device_properties(0, &queueFamilyPropertyCount);

    int feature_count = 0;
    vk_get_logical_device(0, &feature_count);

    my_create_image();
    my_create_render_pass();
    my_create_framebuffer();

    VkShaderModule vertShader = VK_NULL_HANDLE;
    VkShaderModule fragShader = VK_NULL_HANDLE;
    my_create_shader_modules(&vertShader, &fragShader);

    my_create_pipeline(vertShader, fragShader);
    my_create_vertex_buffer();
    my_record_and_submit_draw();

    if (vertShader != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_device, vertShader, nullptr);
    if (fragShader != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_device, fragShader, nullptr);

    vk_cleanup();

    std::cout << "\n============================\n";
    std::cout << "Chapter 8.1 completed successfully.\n";

    return 0;
}

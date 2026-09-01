#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Vulkan tessellation shader demonstration.\n";

    // Initialize Vulkan instance, device, and render targets
    my_init_vulkan();

    // Load shader modules (vertex, tessellation control, tessellation evaluation)
    // Shader modules must be loaded before pipeline creation
    vk_shader_module vertModule = {};
    vk_shader_module tescModule = {};
    vk_shader_module teseModule = {};

    VkResult vkr = vk_create_shader_module("shaders/tessellation.vert.spv", &vertModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Failed to create vertex shader module.\n";
        vk_cleanup();
        return 1;
    }

    vkr = vk_create_shader_module("shaders/tessellation.tesc.spv", &tescModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Failed to create tessellation control shader module.\n";
        vk_cleanup();
        return 1;
    }

    vkr = vk_create_shader_module("shaders/tessellation.tese.spv", &teseModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Failed to create tessellation evaluation shader module.\n";
        vk_cleanup();
        return 1;
    }

    // Create the tessellation graphics pipeline
    my_create_tessellation_pipeline();

    // Demonstrate tessellation (pipeline created and ready)
    my_run_tessellation();

    // Clean up all Vulkan resources
    vk_cleanup();

    std::cout << "Tessellation demonstration complete.\n";
    return 0;
}

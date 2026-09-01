#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Vulkan displacement mapping demonstration.\n";

    my_init_vulkan();

    vk_shader_module vertModule = {};
    vk_shader_module tescModule = {};
    vk_shader_module teseModule = {};

    VkResult vkr = vk_create_shader_module("shaders/displacement.vert.spv", &vertModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Failed to create vertex shader module.\n";
        vk_cleanup();
        return 1;
    }

    vkr = vk_create_shader_module("shaders/displacement.tesc.spv", &tescModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Failed to create tessellation control shader module.\n";
        vk_cleanup();
        return 1;
    }

    vkr = vk_create_shader_module("shaders/displacement.tese.spv", &teseModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Failed to create displacement evaluation shader module.\n";
        vk_cleanup();
        return 1;
    }

    my_create_displacement_pipeline();
    my_run_displacement();

    vk_cleanup();

    std::cout << "Displacement mapping demonstration complete.\n";
    return 0;
}
